#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/InterOptimizer.hpp>

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

Member::Lock Member::readLock() {
    return Member::Lock(false, sharedSelf());
}

Member::Lock Member::writeLock() {
    return Member::Lock(true, sharedSelf());
}

Member::ParallelLock Member::readLockMany(const std::vector<SharedMember<Member>> &plus) {

    while (true) {
        int unwind = -2;
        for (int i = -1; i < plus.size(); i++) {
            auto &mutex = (i == -1) ? wmutex_ : plus.at(i)->wmutex_;

            if (not mutex.try_lock()) {
                unwind = i;
                break;
            }
        }

        if (unwind == -2) {
            // Hurray, we got all the locks!
            break;
        }

        // Unlock all the locks we acquired up to unwind
        if (unwind > -1) {
            wmutex_.unlock();
            for (int i = 0; i < unwind; i++) {
                plus.at(i)->wmutex_.unlock();
            }
        }

        // Now block waiting for unwind's lock, then release it and repeat the whole procedure.
        auto &lock = (unwind == -1) ? wmutex_ : plus.at(unwind)->wmutex_;
        lock.lock();
        lock.unlock();
    }

    // We've got all the locks; increment readlocks for all the members, then release the locks.
    readlocks_++;
    wmutex_.unlock();

    for (auto &m : plus) {
        m->readlocks_++;
        m->wmutex_.unlock();
    }

    return Member::ParallelLock(false, sharedSelf(), plus);
}

Member::ParallelLock Member::writeLockMany(const std::vector<SharedMember<Member>> &plus) {

    while (true) {
        int unwind = -2;
        for (int i = -1; i < plus.size(); i++) {
            auto &lock = (i == -1) ? wmutex_ : plus.at(i)->wmutex_;
            auto &readlocks = (i == -1) ? readlocks_ : plus.at(i)->readlocks_;

            // Try to get a write lock, then check to see if readlocks_ is 0.  If we fail to get the
            // lock, or readlocks_ > 0, we need to unwind any locks before waiting for this particular object's
            // readlocks_ to hit 0.
            if (lock.try_lock()) {
                if (readlocks > 1) {
                    lock.unlock();
                    unwind = i;
                    break;
                }
            }
            else {
                unwind = i;
                break;
            }
        }

        if (unwind == -2) {
            // Hurray, we got all the locks!
            break;
        }

        // Unlock all the locks we acquired up to unwind
        if (unwind > -1) {
            wmutex_.unlock();
            for (int i = 0; i < unwind; i++) {
                plus.at(i)->wmutex_.unlock();
            }
        }

        // Now block waiting for unwind's lock, and then wait for unwind's read locks to finish
        auto &lock = (unwind == -1) ? wmutex_ : plus.at(unwind)->wmutex_;
        auto &readlocks = (unwind == -1) ? readlocks_ : plus.at(unwind)->readlocks_;
        auto &cv = (unwind == -1) ? readlock_cv_ : plus.at(unwind)->readlock_cv_;
        lock.lock();
        cv.wait(lock, [&] { return readlocks == 0; });

        // The object giving us problems is free; unlock and repeat.
        lock.unlock();
    }

    // We've got all the locks, so we're done
    return Member::ParallelLock(true, sharedSelf(), plus);
}

void Member::lock_(bool write) {
    //std::cout << __FILE__ << ":" << __FUNCTION__ << "():" << __LINE__ << "\n" << std::flush;
    wmutex_.lock();
    //std::cout << __FILE__ << ":" << __FUNCTION__ << "():" << __LINE__ << "\n" << std::flush;
    if (write) {
    //std::cout << __FILE__ << ":" << __FUNCTION__ << "():" << __LINE__ << "\n" << std::flush;
        // Obtaining a write lock requires that there by no active read locks, so if there are, we
        // need to wait on readlock_cv_ for read locks to notify us when they finish.
        if (readlocks_ != 0) {
            readlock_cv_.wait(wmutex_, [&] { return readlocks_ == 0; });
        }
    }
    else {
        // Record the readlock, then release the write lock right away.  Until readlocks get
        // decremented in unlock_, no write locks will be granted, but other read locks (which
        // further increment readlocks) are okay.
        readlocks_++;
        wmutex_.unlock();
    }
}

void Member::unlock_(bool write) {
    // Finishing a write lock is easy: just release the lock
    if (write) wmutex_.unlock();
    // To finish a read lock obtain the lock, decrement the number of outstanding read locks, and
    // release the lock.
    else {
        wmutex_.lock();
        readlocks_--;
        // Do a notify_all instead of a notify_one because it's possible that a thread waiting for a
        // write lock is waiting for multiple locks, and thus might not be able to use this
        // notification--in such a case, we need to be sure that other threads waiting for a lock
        // get notified, as they might be able to proceed.
        readlock_cv_.notify_all();
        wmutex_.unlock();
    }
}

void Member::unlock_many_(bool write, const std::vector<SharedMember<Member>> &plus) {
    if (write) {
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        wmutex_.unlock();
        for (auto &member : plus) {
            //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
            member->wmutex_.unlock();
        }
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
    }
    else {
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        wmutex_.lock();
        readlocks_--;
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        readlock_cv_.notify_all();
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        wmutex_.unlock();

        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        for (auto &member : plus) {
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
            member->wmutex_.lock();
            member->readlocks_--;
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
            member->readlock_cv_.notify_all();
            member->wmutex_.unlock();
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
        }
        //std::cout << "unlock_many_():" << __LINE__ << "\n" << std::flush;
    }
}

Member::Lock::Lock(const Member::Lock &l) :
    write(l.write), member(l.member), ptr(l.ptr)
{
    //std::cout << "invoked copy constructor for " << member->id() << " from " << l.member->id() << "\n";
}


Member::Lock::Lock(bool write, SharedMember<Member> member) : write(write), member(member), ptr(new bool(true)) {
    //std::cout << "Obtaining lock\n";
    member->lock_(write);
}

Member::Lock::~Lock() {
    if (ptr.unique()) {
        member->unlock_(write);
        //std::cout << "Releasing lock on " << member->id() << "!\n" << std::flush;
    }
//    else { std::cout << "Not releasing lock: a Lock copy still exists\n" << std::flush; }
}

Member::ParallelLock::ParallelLock(const Member::ParallelLock &pl) :
    write(pl.write),
    member(pl.member),
    plus_ptr(pl.plus_ptr)
{}

Member::ParallelLock::ParallelLock(bool write, SharedMember<Member> member, const std::vector<SharedMember<Member>> &plus)
    : write(write), member(member), plus_ptr(new std::vector<SharedMember<Member>>(plus)) {
        /*std::cout << "Obtained big lock on: " << member->id();
        for (auto &m : *plus_ptr) {
            std::cout << ", " << m->id();
        }
        std::cout << "\n" << std::flush;*/
}

Member::ParallelLock::~ParallelLock() {
    //std::cout << "PL destroyed\n" << std::flush;
    if (plus_ptr.unique()) {
        try {
            member->unlock_many_(write, *plus_ptr);
        } catch (std::exception &e) {
            std::cerr << "Caught unlock exception: " << e.what() << std::flush;
            throw;
        }
        //std::cout << "Released big lock\n" << std::flush;
    }
    //else { std::cout << "Not releasing plock: a ParallelLock copy still exists\n" << std::flush; }
}

}
