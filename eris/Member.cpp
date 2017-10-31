#include <eris/Member.hpp>
#include <system_error>
#include <vector>

namespace eris {

std::atomic<id_t> Member::next_id_{1};

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

Member::Lock::Lock(bool write, bool locked) : Lock{write, locked, std::set<SharedMember<Member>>{}} {}
Member::Lock::Lock(bool write, std::set<SharedMember<Member>> &&members)
    : data{std::make_shared<Data>(std::move(members), write)} {
    lock();
}
Member::Lock::Lock(bool write, bool locked, std::set<SharedMember<Member>> &&members)
    : data{std::make_shared<Data>(std::move(members), write, locked)} {}

Member::Lock::Lock(const Lock &l) : data{l.data} {}

Member::Lock::~Lock() {
    if (data.unique() && isLocked()) unlock();
}

bool Member::Lock::lock_all_(bool write, bool only_try) {
    // Not currently locked
    auto &members = data->members;
    auto mem_begin = members.begin();
    auto mem_end = members.end();
    auto holding_it = mem_end;

    // The loops below work like this:
    // - go through the list of members one-by-one, trying to obtain a lock as we go.
    //   - if we fail to obtain a lock:
    //     - release all the locks obtained in the loop iterations before the one that failed
    //     - obtain a lock (with blocking) on the one that failed
    //     - once that lock is obtained, redo the loop.  (The one already held is skipped.)
    //     - repeat the above (Note 1: if the loop fails to obtain the lock *before* the one already
    //       held, we have to additionally release that lock).  (Note 2: if `only_try` is true, we
    //       don't repeat here, but instead return false after releasing all the locks).
    // The whole thing repeats until we get all the way through the loop without any lock failures:
    // at this point, we have a lock on everything and can return.

    while (true) {
        auto unwind_it = mem_end;
        for (auto it = mem_begin; it != mem_end; ++it) {
            if (holding_it == it) continue; // We're already holding this mutex lock from the previous attempt

            if (!((*it)->try_lock_(write))) {
                // If we didn't get the lock, we need to release all the locks previously obtained in
                // this loop:
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
            (*undoit)->unlock_(write);
            if (undoit == holding_it) undid_holding = true;
        }

        // If we're also holding a lock from the previous time through that didn't just get undone
        // (i.e. last time we got stuck on lock `i`, this time we got stuck on lock `j < i`),
        // release it too
        if (holding_it != mem_end && !undid_holding) {
            (*holding_it)->unlock_(write);
            holding_it = mem_end;
        }

        // If we're only trying, we failed, so return false.
        if (only_try) return false;

        // Now block waiting for unwind's lock, then repeat the whole procedure.
        (*unwind_it)->lock_(write);

        // We've now got a lock on the member that was giving us trouble while trying to get all the
        // locks without blocking, so try again.
        holding_it = unwind_it;
    }

    return true;
}


bool Member::Lock::read(bool only_try) {
    if (isFake()) {
        // No members, this is a fake lock
        data->write = false;
        data->locked = true;
        return true;
    }

    if (isLocked()) {
        if (isRead())
            return true; // Nothing to do
        else
            unlock(); // Currently a write lock: release it first
    }

    data->write = false;
    data->locked = lock_all_(false, only_try);
    return isLocked();
}

bool Member::Lock::write(bool only_try) {
    if (isFake()) {
        data->write = true;
        data->locked = true;
        return true;
    }

    if (isLocked()) {
        if (isWrite()) // Already an active write lock, nothing to do.
            return true;
        else
            unlock(); // Currently a read lock: release it first
    }

    data->write = true;
    data->locked = lock_all_(true, only_try);
    return isLocked();
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
    if (!isLocked()) throw std::system_error(std::make_error_code(std::errc::operation_not_permitted),
            "Member::Lock::unlock: not locked");
    if (!isFake()) {
        const bool write = isWrite();
        for (auto &m : data->members)
            m->unlock_(write);
    }
    data->locked = false;
}

void Member::Lock::transfer(Member::Lock &from) {
    if (isWrite() != from.isWrite() || isLocked() != from.isLocked()) {
        throw Member::Lock::mismatch_error();
    }

    // Copy from's members
    data->members.insert(from.data->members.begin(), from.data->members.end());
    // Delete from's members
    from.data->members.clear();
}

bool Member::Lock::try_add(const SharedMember<Member> &member) {
    if (isFake())
        return true;

    if (isLocked()) {
        // First try to see if we can obtain a (non-blocking) lock on the new member.  If we can, great,
        // just hold that lock and add the new member to the set of locked members.  If not, we have to
        // release the existing lock, add the new one into the member list, then do a blocking lock on
        // the entire (old + new) set of members.
        if (!(isWrite() ? member->mutex_.try_lock() : member->mutex_.try_lock_shared())) {
            // Couldn't get the required lock; we'll have to release all and do a full blocking lock
            return false;
        }
    }

    // Either we successfully locked the new member, or the lock isn't active so we can add:
    data->members.insert(member);
    return true;
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
