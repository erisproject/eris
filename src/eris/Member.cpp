#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>

namespace eris {

void Member::dependsOn(const eris_id_t &id) {
    simulation()->registerDependency(*this, id);
}


void Member::simulation(const std::shared_ptr<Simulation> &sim, const eris_id_t &id) {
    if (id == 0) removed();
    simulation_ = sim;
    id_ = id;
    if (id > 0) added();
}


std::shared_ptr<Simulation> Member::simulation() const {
    return simulation_.lock();
}

void Member::added() {}
void Member::removed() {}

Member::Lock::Lock(bool write) : Lock(write, std::vector<SharedMember<Member>>()) {}
Member::Lock::Lock(bool write, SharedMember<Member> member) : Lock(write, std::vector<SharedMember<Member>>(1, member)) {}
Member::Lock::Lock(bool write, std::vector<SharedMember<Member>> &&members)
    : data(new Data(std::forward<std::vector<SharedMember<Member>>>(members), write)) {

    lock();
}

Member::Lock::Lock(const Lock &l) : data(l.data) {}

Member::Lock::~Lock() {
    if (data.unique()) release();
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
            // Write lock established; downgrade all locks to read locks
            for (auto &m : data->members) {
                m->readlocks_++;
                m->wmutex_.unlock();
            }
            data->write = false;
        }
        // Otherwise we don't care: calling read() on an active readlock does nothing.
    }
    else {
        // Not currently locked
        int holding = -1;
        auto &members = data->members;
        while (true) {
            int unwind = -1;
            for (int i = 0; i < (int) members.size(); i++) {
                if (holding == i) continue; // We're already holding this lock

                auto &mutex = members.at(i)->wmutex_;

                if (not mutex.try_lock()) {
                    unwind = i;
                    break;
                }
            }

            if (unwind == -1) {
                // Hurray, we got all the locks!
                break;
            }

            // Otherwise unlock all the locks we acquired up to unwind
            for (int i = 0; i < unwind; i++) {
                members.at(i)->wmutex_.unlock();
            }
            // If we're also holding a lock from the previous time through that didn't just get
            // undone, release it too
            if (holding >= unwind)
                members.at(holding)->wmutex_.unlock();

            // Now block waiting for unwind's lock, then repeat the whole procedure.
            members.at(unwind)->wmutex_.lock();
            holding = unwind;
        }

        for (auto &m : members) {
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


    int holding = -1;
    auto &members = data->members;
    while (true) {
        int unwind = -1;
        for (int i = 0; i < (int) members.size(); i++) {
            if (i == holding) continue; // We're already holding this write lock

            auto &lock = members.at(i)->wmutex_;
            auto &readlocks = members.at(i)->readlocks_;

            // Try to get a mutex lock, then check to see if readlocks_ is 0: if we get a lock on
            // wmutex_ but readlocks_ is non-zero, we don't have an exclusive (write) lock yet, so
            // back off.  If we fail to get the exclusive lock (because wmutex_ is held, or because
            // readlocks_ > 0), we need to unwind any locks we already got up to this point.  Then
            // we'll wait on the one we didn't get, then try the whole thing again.
            if (lock.try_lock()) {
                if (readlocks > 0) {
                    lock.unlock();
                    unwind = i;
                    break;
                }
                // Else we got the lock and readlocks is 0: i.e. we got the desired write lock; just
                // hold onto it and keep going.
            }
            else {
                // Failed to obtain a mutex lock, so unwind any we did get along the way
                unwind = i;
                break;
            }
        }

        if (unwind == -1) {
            // If unwind is still -1, we got through the entire loop without a lock failure, so
            // we're done.
            break;
        }

        // Otherwise, we have to unlock all the mutex locks we aquired up to (but not including) `unwind':
        for (int i = 0; i < unwind; i++) {
            members.at(i)->wmutex_.unlock();
        }
        // If we're also holding a lock from the previous time through that didn't just get
        // undone, release it too
        if (holding >= unwind)
            members.at(holding)->wmutex_.unlock();

        // Now wait on  `unwind's lock mutex until it is granted *and* readlocks_ hits 0
        members.at(unwind)->wmutex_.lock();
        members.at(unwind)->readlock_cv_.wait(members.at(unwind)->wmutex_,
                [&] { return members.at(unwind)->readlocks_ == 0; });

        // The object giving us problems is free, so let's try the whole thing again
        holding = unwind;
    }

    // We have exclusive locks on everything: great!
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

}
