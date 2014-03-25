#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>

namespace eris {

void Member::dependsOn(const eris_id_t &id) {
    simulation()->registerDependency(*this, id);
}

void Member::dependsWeaklyOn(const eris_id_t &id) {
    simulation()->registerWeakDependency(*this, id);
}

void Member::weakDepRemoved(SharedMember<Member>, const eris_id_t&) {}


SharedMember<Member> Member::sharedSelf() const { return simOther<Member>(id()); }

void Member::simulation(const std::shared_ptr<Simulation> &sim, const eris_id_t &id) {
    if (id == 0) removed();
    simulation_ = sim;
    id_ = id;
    if (id > 0) added();
}

bool Member::hasSimulation() const {
    return !(simulation_.expired());
}

std::shared_ptr<Simulation> Member::simulation() const {
    if (simulation_.expired()) {
        throw no_simulation_error();
    }
    return simulation_.lock();
}

void Member::added() {}
void Member::removed() {}

Member::Lock::Lock(bool write, bool locked) : Lock(write, locked, std::multiset<SharedMember<Member>>()) {}
Member::Lock::Lock(bool write, std::multiset<SharedMember<Member>> &&members)
    : data(new Data(std::forward<std::multiset<SharedMember<Member>>>(members), write)) {
    lock();
}
Member::Lock::Lock(bool write, bool locked, std::multiset<SharedMember<Member>> &&members)
    : data(new Data(std::forward<std::multiset<SharedMember<Member>>>(members), write, locked)) {}

Member::Lock::Lock(const Lock &l) : data(l.data) {}

Member::Lock::~Lock() {
    if (data.unique()) release();
}

void Member::Lock::mutex_lock_(bool write) {
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
}


void Member::Lock::read() {
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
        mutex_lock_(false);

        for (auto &m : data->members) {
            m->readlocks_++;
            m->wmutex_.unlock();
        }

        data->locked = true;
        data->write = false;
    }
}

void Member::Lock::write() {
    if (data->members.empty()) {
        // Fake lock
        data->write = true;
        data->locked = true;
        return;
    }

    if (isLocked()) {
        if (isWrite()) // Already an active write lock, nothing to do.
            return;
        else // Read lock: release it first
            release();
    }

    mutex_lock_(true); // When this returns, we have an exclusive lock on all members

    data->locked = true;
    data->write = true;
}

void Member::Lock::lock() {
    if (isLocked()) return; // Already locked
    if (isWrite()) write();
    else read();
}

void Member::Lock::release() {
    if (!isLocked()) return; // Already released
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
        // a write lock--which isn't possible if we're a read lock--and momentary lock attempts,
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

void Member::Lock::add(SharedMember<Member> member) {

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

    if (need_release) {
        // We failed to obtain a lock on the new member, so we need to release the current lock,
        // add the new member, then try for a lock on all members.
        release();
        data->members.insert(member);
        lock();
    }
    else {
        // Either the lock isn't active, or we got the lock without blocking, so all that's left is
        // adding the new member into the list of locked members.
        data->members.insert(member);
    }
}

Member::Lock Member::Lock::remove(SharedMember<Member> member) {
    return remove(std::vector<SharedMember<Member>>(1, member));
}

}
