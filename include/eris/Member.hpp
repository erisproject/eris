#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <stdexcept>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>

#include <iostream>

namespace eris {

class Simulation;
class Agent;
class Good;
class Market;
class IntraOptimizer;
class InterOptimizer;

/** Base class for "members" of a simulation: goods, agents, markets, and optimizers.  This class
 * provides an id, a weak reference to the owning simulation object, and a < operator comparing ids
 * (so that ordering works).
 */
class Member {
    public:
        virtual ~Member() { std::cout << "~Member\n" << std::flush; } //= default;
        /** Returns the eris_id_t ID of this member.  Returns 0 if this Member instance has not yet
         * been added to a Simulation.
         */
        eris_id_t id() const { return id_; }
        /** A Member object can be used anywhere an eris_id_t value is called for and will evaluate
         * to the Member's ID.
         */
        operator eris_id_t() const { return id_; }

        /** Creates and returns a shared pointer to the simulation this object belongs.  Returns a
         * null shared_ptr if there is no (current) simulation.
         */
        virtual std::shared_ptr<Simulation> simulation() const;

        /** Shortcut for `member.simulation()->agent<A>()` */
        template <class A = Agent> SharedMember<A> simAgent(eris_id_t aid) const;
        /** Shortcut for `member.simulation()->good<G>()` */
        template <class G = Good> SharedMember<G> simGood(eris_id_t gid) const;
        /** Shortcut for `member.simulation()->market<M>()` */
        template <class M = Market> SharedMember<M> simMarket(eris_id_t mid) const;
        /** Shortcut for `member.simulation()->intraOpt<I>()` */
        template <class I = IntraOptimizer> SharedMember<I> simIntraOpt(eris_id_t oid) const;
        /** Shortcut for `member.simulation()->interOpt<I>()` */
        template <class I = InterOptimizer> SharedMember<I> simInterOpt(eris_id_t oid) const;

        /** Records a dependency with the Simulation object.  This should not be called until after
         * simulation() has been called, and is typically invoked in an overridden added() method.
         *
         * This is simply an alias for Simulation::registerDependency; the two following statements
         * are exactly equivalent:
         *
         *     shared_member->dependsOn(other_member);
         *     shared_member->simulation()->registerDependency(shared_member, other_member);
         */
        void dependsOn(const eris_id_t &id);

        /** Simple lock class returned by readLock() and writeLock().  This class has no public
         * methods: it obtains a lock during object construction, and releases the lock during
         * destruction.
         *
         * Locks may be duplicated, in which case the lock will persist until all copies of the Lock
         * object are destroyed.
         *
         * It is not recommended to access a Lock object from multiple threads.
         *
         * Implementation details: in both lock cases (read or write), we first obtain a write lock
         * on the object's mutex.  In the case of a readlock, we then increment the object's
         * readlocks variable and then immediately release the lock.  If obtaining a write lock, we
         * check readlocks: if not 0, we use a condition variable to release the write lock until
         * readlocks goes to 0, at which point construction is complete (the mutex lock is held).
         *
         * When unlocking a read lock (i.e. during object destruction), we decrement readlocks and
         * notify on the calling object's condition variable (to notify any write locks that might
         * be waiting for all read locks to run out).  When unlocking a write lock, we simply
         * release the mutex lock.
         */
        class Lock final {
            public:
                /** Default constructor explicitly deleted. */
                Lock() = delete;
                /** Copy constructor; both copy and original must be destroyed before the lock is
                 * released. */
                Lock(const Lock &l);
                /** Releases the held lock (unless there are still living copies of the Lock object) */
                ~Lock();
            private:
                /** Obtains a lock.  If `write` is true, the lock is an exclusive write-lock; if
                 * false, it is a read-lock which is permitted to coexist with other read locks (but
                 * not write locks).
                 */
                Lock(bool write, SharedMember<Member> member);
                friend class Member;

                const bool write;
                SharedMember<Member> member;
                std::shared_ptr<bool> ptr;
        };

        /** A class that holds multiple Member locks at once.  The locks are released when the
         * object is destroyed.  A ParallelLock may be copied, in which case all copies must be
         * destroyed before the lock is released.
         *
         * It is not recommended to access a Lock object from multiple threads.
         */
        class ParallelLock final {
            public:
                /** Default constructor explicitly deleted. */
                ParallelLock() = delete;
                /** Copy constructor. Lock release happens once all copies are destroyed. */
                ParallelLock(const ParallelLock &pl);
                /** Releases the held locks. */
                ~ParallelLock();
            private:
                ParallelLock(bool write, SharedMember<Member> member, const std::vector<SharedMember<Member>> &plus);
                friend class Member;

                const bool write;
                SharedMember<Member> member;
                std::shared_ptr<const std::vector<SharedMember<Member>>> plus_ptr;
        };

        /** Obtains a read lock for the current object, returns a Member::Lock object.  The read
         * lock lasts until the returned unique_ptr goes out of scope (and thus the Member::Lock
         * object is destroyed).  A read-lock is a promise that no value will changed while the read
         * lock is held.  Value-changing code should obtain a write lock before changing values so
         * as to honour this promise.
         *
         * Not: not capturing the returned object is a serious bug as the lock is released
         * immediately.
         *
         * Multiple read locks may be active at one time, however a write lock will not be granted
         * until all read locks have been released; likewise read locks have to wait until the
         * active write lock is released.
         *
         * There is no guarantee to the order of the lock, i.e. lock requests (whether read or write
         * locks) may be serviced in any order.
         */
        Lock readLock() const;

        /** Obtains a read/write lock for the current object, returning a unique_ptr<Member::Lock>
         * object.  The write lock lasts until the returned unique_ptr goes out of scope (and thus
         * the Member::Lock object is destroyed).  Note: not capturing the returned object is a
         * serious bug as the lock is released immediately.
         *
         * There is no guarantee to the order of the lock, i.e. lock requests (whether read or write
         * locks) may be serviced in any order.
         *
         * You can safely (i.e. without deadlocking) obtain multiple write locks on the same object
         * from the same thread, but attempting to obtain a write lock over top of a read lock in
         * the same thread will deadlock: don't do that.  You may also safely obtain read locks over
         * top of write locks so long as no write lock is obtained until all the thread's read locks
         * are released.
         *
         * This held lock will always be an exclusive lock, that is no other read locks or write
         * locks will be active.
         */
        Lock writeLock();

        /** Obtains a read lock for the current object *plus* all the objects passed in.  This will
         * block until a read lock can be obtained on all objects.
         *
         * This method is designed to be deadlock safe: it will not block while holding any lock; if
         * unable to obtain a lock on one of the objects, it will release any other held locks
         * before blocking on the unobtainable lock.
         *
         * Because the fundamental mutex used for locking is recursive, it is safe to call this in
         * such a way that objects are locked multiple times.
         *
         * The lock will be released when the returned object is destroyed, typically by going out
         * of scope.
         */
        ParallelLock readLockMany(const std::vector<SharedMember<Member>> &plus) const;

        /** Obtains a write lock for the current object *plus* all the objects passed in.  This will
         * block until a write lock can be obtained on all objects.
         *
         * Like readLockMany, this method is deadlock safe if used properly: it will not block
         * waiting for a lock while holding any other locks, and, because of the use of a recursive
         * mutex, will not deadlock when an object is included multiple times in the list of objects
         * to lock.
         *
         * Overlapping write locks on the same object within the same thread are allowed, but write
         * locks on objects that are read-locked in the same thread will cause a deadlock.  You may
         * overlap the other way (i.e. obtaining a read lock on a write-locked object), so long as
         * you never try to obtain a write lock over top of the read lock).
         *
         * The lock will be released when the returned object is destroyed, typically by going out
         * of scope.
         */
        ParallelLock writeLockMany(const std::vector<SharedMember<Member>> &plus);

    protected:
        /** Called (by Simulation) to store a weak pointer to the simulation this member belongs to
         * and the member's id.  This is called once when the Member is added to a simulation, and
         * again (with a null shared pointer and id of 0) when the Member is removed from a
         * Simulation.
         */
        void simulation(const std::shared_ptr<Simulation> &sim, const eris_id_t &id);
        friend eris::Simulation;

        /** Virtual method called just after the member is added to a Simulation object.  The
         * default implementation does nothing.  This method is typically used to record a
         * dependency in the simulation, but can also do initial setup.
         */
        virtual void added();

        /** Virtual method called just after the member is removed from a Simulation object.  The
         * default implementation does nothing.  The simulation() and id() methods are still
         * available, but the simulation no longer references this Member.
         *
         * Note that any registered dependencies may not exist (in particular when the removal is
         * occuring as the result of a cascading dependency removal).  In other words, if this
         * Member has registered a dependency on B, when B is removed, this Member will also be
         * removed *after* B has been removed from the Simulation.  Thus classes overriding this
         * method must *not* make use of dependent objects.
         */
        virtual void removed();

        /** Helper method to ensure that the passed-in SharedMember is a subclass of the templated
         * class.  If not, this throws an invalid_argument exception with the given message.
         */
        template<class B, class C>
        void requireInstanceOf(const SharedMember<C> &obj, const std::string &error) {
            if (!dynamic_cast<B*>(obj.ptr.get())) throw std::invalid_argument(error);
        }

        /** Returns a SharedMember wrapper around the current object, obtained through the
         * Simulation object (so that the shared_ptr is properly shared with everything else that
         * has a SharedMember pointing to the same object).
         *
         * This returns a generic SharedMember<Member>, which is castable to SharedMember<O> where O
         * is the actual O subclass the object belongs to.
         */
        virtual SharedMember<Member> sharedSelf() const = 0;


    private:
        eris_id_t id_ = 0;
        /** Stores a weak pointer to the simulation this Member belongs to. */
        std::weak_ptr<eris::Simulation> simulation_;

        /** Mutex used both for a write lock and instantaneously for a read lock.  Also guards
         * access to readlocks_ */
        mutable std::recursive_mutex wmutex_;

        /** Number of currently active read locks.  While this is > 0, no write lock will be
         * granted.
         */
        mutable unsigned int readlocks_{0};
        /** A condition_variable that is used to notify waiting threads when a read lock is released.
         */
        std::condition_variable_any readlock_cv_;

        /** Called by the Lock nested class during construction to establish a lock on this object.
         */
        void lock_(bool write);
        /** Called when a Lock object is destroyed to release the lock it held.
         */
        void unlock_(bool write);

        /** Called during ParallelLock destruction to release the locks on the current object and
         * provided vector of objects
         */
        void unlock_many_(bool write, const std::vector<SharedMember<Member>> &plus);
};

}

// This has to be down here because there is a circular dependency between Simulation and Member:
// each has templated methods requiring the definition of the other.
#include <eris/Simulation.hpp>

namespace eris {
template <class A> SharedMember<A> Member::simAgent(eris_id_t aid) const {
    return simulation()->agent<A>(aid);
}
template <class G> SharedMember<G> Member::simGood(eris_id_t gid) const {
    return simulation()->good<G>(gid);
}
template <class M> SharedMember<M> Member::simMarket(eris_id_t mid) const {
    return simulation()->market<M>(mid);
}
template <class I> SharedMember<I> Member::simIntraOpt(eris_id_t oid) const {
    return simulation()->intraOpt<I>(oid);
}
template <class I> SharedMember<I> Member::simInterOpt(eris_id_t oid) const {
    return simulation()->interOpt<I>(oid);
}
}
