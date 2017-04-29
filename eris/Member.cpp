#include <eris/Member.hpp>
#include <system_error>
#include <vector>

namespace eris {

std::atomic<eris_id_t> Member::next_id_{1};

void Member::dependsOn(MemberID dep_id) {
    simulation()->registerDependency(id(), dep_id);
}

void Member::dependsWeaklyOn(MemberID dep_id) {
    simulation()->registerWeakDependency(id(), dep_id);
}

void Member::weakDepRemoved(SharedMember<Member>) {}


SharedMember<Member> Member::sharedSelf() const { return simOther(id()); }

void Member::simulation(const std::shared_ptr<Simulation> &sim) {
    if (!sim) removed();
    simulation_ = sim;
    if (sim) added();
}

bool Member::hasSimulation() const {
    return !(simulation_.expired());
}

void Member::added() {}
void Member::removed() {}

Member::Lock::Lock(bool write, bool locked) : Lock{write, locked, std::multiset<SharedMember<Member>>{}} {}
Member::Lock::Lock(bool write, std::multiset<SharedMember<Member>> &&members)
    : data{std::make_shared<Data>(std::move(members), write)} {
    lock();
}
Member::Lock::Lock(bool write, bool locked, std::multiset<SharedMember<Member>> &&members)
    : data{std::make_shared<Data>(std::move(members), write, locked)} {}

Member::Lock::Lock(const Lock &l) : data{l.data} {}

Member::Lock::~Lock() {
    if (data.unique() and isLocked()) unlock();
}

bool Member::Lock::mutex_lock_(bool write, bool only_try) {
    // Not currently locked
    auto &members = data->members;
    auto mem_begin = members.begin();
    auto mem_end = members.end();
    auto holding_it = mem_end;
    while (true) {
        auto unwind_it = mem_end;
        for (auto it = mem_begin; it != mem_end; ++it) {
            if (holding_it == it) continue; // We're already holding this mutex lock from the previous attempt

            auto &mutex = (*it)->wmutex_;

            bool got_lock = mutex.try_lock();
            // If we're getting a write lock, we need to check not only that we got the mutex lock,
            // but also that we got it when readlocks_ equals 0; otherwise, we only really have a
            // shared (i.e. read) lock, so need to release it and try again.
            if (write and got_lock and (*it)->readlocks_ > 0) {
                mutex.unlock();
                got_lock = false;
            }

            if (not got_lock) {
                unwind_it = it;
                break;
            }
        }

        if (unwind_it == mem_end) {
            // Hurray, we got all the locks!
            break;
        }

        bool undid_holding = false;

        // Otherwise unwind from the beginning up to the unwind iterator, unlocking all the
        // locks we acquired
        for (auto undoit = mem_begin; undoit != unwind_it; ++undoit) { // Undo [begin,unwind)
            auto &mem = *undoit;
            mem->wmutex_.unlock();
            if (undoit == holding_it) undid_holding = true;
        }

        // If we're also holding a lock from the previous time through that didn't just get
        // undone, release it too
        if (holding_it != mem_end and not undid_holding) {
            (*holding_it)->wmutex_.unlock();
            holding_it = mem_end;
        }

        auto unwind = *unwind_it;

        // If we're only trying, we failed, so return false.
        if (only_try) return false;

        // Now block waiting for unwind's lock, then repeat the whole procedure.
        unwind->wmutex_.lock();

        // If we're looking for a readlock, we need to release and wait on the readlocks_cv_ until
        // we get the lock *and* readlocks_ == 0.
        if (write) {
            unwind->readlock_cv_.wait(unwind->wmutex_, [&] { return unwind->readlocks_ == 0; });
        }

        // We've now got a lock on the member that was giving us trouble while trying to get all the
        // locks without blocking, so try again.
        holding_it = unwind_it;
    }

    return true;
}


bool Member::Lock::read(bool only_try) {
    if (data->members.empty()) {
        // No members, this is a fake lock
        data->write = false;
        data->locked = true;
    }
    else if (isLocked()) {
        // Already locked
        if (isWrite()) {
            // Write lock already established; downgrade all locks to read locks by incrementing
            // readlocks_ and releasing the member mutexes
            for (auto &m : data->members) {
                m->readlocks_++;
                m->wmutex_.unlock();
            }
            data->write = false;
        }
        // Otherwise we don't care: calling read() on an active readlock does nothing.
    }
    else {
        bool got_lock = mutex_lock_(false, only_try);
        data->write = false;
        data->locked = got_lock;

        if (!got_lock) return false;

        for (auto &m : data->members) {
            m->readlocks_++;
            m->wmutex_.unlock();
        }
    }
    return true;
}

bool Member::Lock::write(bool only_try) {
    if (data->members.empty()) {
        // Fake lock
        data->write = true;
        data->locked = true;
        return true;
    }

    if (isLocked()) {
        if (isWrite()) // Already an active write lock, nothing to do.
            return true;
        else // Read lock: release it first
            unlock();
    }

    bool got_lock = mutex_lock_(true, only_try);
    data->write = true;
    data->locked = got_lock;

    return got_lock;
}

void Member::Lock::lock() {
    if (isLocked()) throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur),
            "Member::Lock::lock: already locked");
    if (isWrite()) write();
    else read();
}

bool Member::Lock::try_lock() {
    if (isLocked()) throw std::system_error(std::make_error_code(std::errc::resource_deadlock_would_occur),
            "Member::Lock::lock: already locked");
    return isWrite() ? write(true) : read(true);
}

void Member::Lock::unlock() {
    if (not isLocked()) throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
            "Member::Lock::unlock: not locked");
    if (data->members.empty()) {
        // Fake lock
    }
    else if (isWrite()) {
        // To release a write lock, simply release all the currently held mutex locks
        for (auto &m : data->members)
            m->wmutex_.unlock();
    }
    else {
        // Releasing a read lock requires relocking the mutex, decrementing readlocks_, then
        // releasing the mutex, repeated for each object.  (Since nothing ever holds a mutex except
        // a write lock (which isn't possible if we're a read lock) and a momentary lock attempt,
        // this may block for brief periods of time, but won't deadlock.
        for (auto &m : data->members) {
            m->wmutex_.lock();
            bool notify = (--(m->readlocks_) == 0);
            if (notify)
                m->readlock_cv_.notify_all();
            m->wmutex_.unlock();
        }
    }
    data->locked = false;
}

bool Member::Lock::isLocked() {
    return data->locked;
}

bool Member::Lock::isWrite() {
    return data->write;
}

void Member::Lock::transfer(Member::Lock &from) {
    if (isWrite() != from.isWrite() or isLocked() != from.isLocked()) {
        throw Member::Lock::mismatch_error();
    }

    // Copy from's members
    data->members.insert(from.data->members.begin(), from.data->members.end());
    // Delete from's members
    from.data->members.clear();
}

bool Member::Lock::try_add(const SharedMember<Member> &member) {

    bool need_release = false; // Will be true if we fail to get a non-blocking lock

    if (isLocked()) {
        // First try to see if we can obtain a (non-blocking) lock on the new member.  If we can, great,
        // just hold that lock and add the new member to the set of locked members.  If not, we have to
        // release the existing lock, add the new one into the member list, then do a blocking lock on
        // the entire (old + new) set of members.
        auto &mutex = member->wmutex_;
        if (mutex.try_lock()) {
            // Got the mutex lock right away
            if (isWrite()) {
                // But we need a write lock
                if (member->readlocks_ > 0) {
                    // Outstanding read locks, so we didn't really get a write lock
                    mutex.unlock();
                    need_release = true;
                }
            }
            else {
                // Read lock
                member->readlocks_++;
                mutex.unlock();
            }
        }
        else {
            // The mutex lock would blocked, so we need to release everything before going for the
            // blocking lock.
            need_release = true;
        }
    }

    if (need_release)
        return false;
    else {
        data->members.insert(member);
        return true;
    }
}

void Member::Lock::add(const SharedMember<Member> &member) {
    bool added = try_add(member);

    if (!added) {
        // Adding failed, which means we need to release the current lock, add the new member, then
        // try for a lock on all members.
        unlock();
        data->members.insert(member);
        lock();
    }
    // Otherwise adding the member succeeded.
}

Member::Lock Member::Lock::remove(const SharedMember<Member> &member) {
    std::vector<SharedMember<Member>> to_remove;
    to_remove.push_back(std::move(member));
    return remove(to_remove);
}

Member::Lock::Supplemental::Supplemental(Lock &lock, const SharedMember<Member> &member) : lock_{lock}, members_(1, member) {
    lock_.add(member);
}

Member::Lock::Supplemental::~Supplemental() {
    if (!members_.empty()) lock_.remove(members_);
}

Member::Lock::Supplemental Member::Lock::supplement(const SharedMember<Member> &member) {
    return Supplemental(*this, member);
}

}
