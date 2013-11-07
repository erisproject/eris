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
#include <unordered_map>

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
        virtual ~Member() = default;
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


        /** A locking class for holding one or more simultaneous Member locks.  Locks are
         * established during object construction and released when the object is destroyed.  A
         * Lock may be copied, in which case all copies must be destroyed before the lock is
         * automatically released.
         *
         * When locking multiple objects at once, this class is designed to avoid deadlocks: it will
         * not block while waiting for a lock while it holds any open locks; instead if a lock
         * cannot be obtained, any previous locks are released while waiting for the locked object
         * to become unlocked.  Construction (or a lock() call) do not return until all required
         * locks are held.
         *
         * There are provided public methods for manually releasing and re-obtaining the locks, and
         * methods for converting a read lock into a write lock and vice-versa.  These methods are
         * typically not needed (when object lifetime will suffice), but available for convenience
         * for more precise lock control.
         *
         * It is not recommended (or supported) to access a Lock object from multiple threads.
         *
         * Implementation details:
         *
         * Every Member-derived object has a lock mutex that governs write access to the object.
         * Additionally, each Member has a readlocks variable (access to which requires locking the
         * mutex).  A write lock is thus a case of holding the mutex while readlocks is set to 0.  A
         * read lock is simply an object that has readlocks set to a value greater than 0.
         * Establishing a read lock thus requires locking the mutex, incrementing the readlocks
         * variable, then releasing the mutex lock.
         *
         * To establishing a lock in both cases (read or write), we first try (in a non-blocking
         * way) to obtain mutex locks on all of the Member-derived objects to be locked.  If any of
         * them fails (either because locking the mutex would block, or because need a write lock
         * but when we got the mutex lock the object has readlocks > 0, and thus must wait for
         * outstanding readlock(s) to finish), we unlock any held mutexes then do a blocking lock on
         * the failed mutex.  When we finally get that lock, we restart the process.
         *
         * Once we get through the entire set of mutexes, and thus hold locks on all required
         * objects, if we want a write lock, we're finished.  If we want a read lock, we increment
         * all the objects' readlocks variable, then release all the mutex locks.
         *
         * Unlocking is easier: for a write lock, we simply release all the mutex locks.  For a read
         * lock, we have to briefly a obtain a mutex lock on the object, decrement its readlocks
         * variable, then release the mutex lock.  (Since a Lock object never holds a mutex lock
         * while waiting for something else, these blocking locks are guaranteed to be
         * non-deadlocking).
         */
        class Lock final {
            public:
                /** Default constructor explicitly deleted. */
                Lock() = delete;
                /** Copy constructor.  Automatic lock release happens once all copies are destroyed.
                 * Both also share lock status: releasing one or converting one into a different
                 * type of lock also releases or converts any copies.*/
                Lock(const Lock &l);
                /** Releases the held lock(s) (unless copies of the Lock are still around). */
                ~Lock();
                /** If the lock is currently a read lock (or is currently inactive), calling this
                 * converts it to a write lock.  If the lock is already a write lock, this does
                 * nothing.  Since all copies of this Lock object share the lock status, they will
                 * simultaneously become write locks.
                 *
                 * Note that if other threads currently have a read lock, this will block until all
                 * other locks are released.
                 *
                 * If the lock has been explicitly released (by calling release()), calling this
                 * method reestablishes the lock.
                 */
                void write();
                /** If the lock is currently a write lock, calling this converts it to a read lock.
                 * If the lock is already a read lock, this does nothing.  Since all copies of this
                 * Lock object share the lock status, they will simultaneously become read locks.
                 *
                 * This is approximately equivalent to releasing the current write lock and
                 * obtaining a new read lock, except that it is guaranteed not to block when called
                 * on an active lock: when already a read lock it does nothing; when converting a
                 * write lock to a read lock it establishes the read lock before releasing the held
                 * write lock.
                 *
                 * If the lock has been explicitly released (by calling release()), calling this
                 * method reestablishes the lock and may block.
                 */
                void read();
                /** Obtains the lock.  This is called automatically when the object is created and
                 * when switching lock types, but may also be called manually.  If the lock is
                 * currently active, this has no effect.  Note that this will block if the lock is
                 * currently not available.
                 */
                void lock();
                /** Releases the held lock.  This is called automatically when the object is
                 * destroyed and when switching lock types, but may also be called manually.  If the
                 * lock is already inactive, this has no effect.
                 */
                void release();
                /** Returns true if this lock is currently a write lock, false if it is a read lock.
                 */
                bool isWrite();
                /** Returns true if this lock is currently active (i.e. actually a lock), false
                 * otherwise.  Note that this status is shared among all copies of a Lock object.
                 */
                bool isLocked();
            private:
                /** Constructs a fake lock.  This is equivalent to create a Lock with no members.
                 * This is used by Member.readLock and .writeLock when the simulation doesn't use
                 * threading so avoids all the actual lock code.
                 */
                Lock(bool write);
                /** Creates a lock that applies to a single member. Calls read() or write() before
                 * returning. */
                Lock(bool write, SharedMember<Member> member);
                /** Creates a lock that applies to a vector of members. Calls lock() (which calls
                 * read() or write()) before returning. */
                Lock(bool write, std::vector<SharedMember<Member>> &&members);

                friend class Member;

                class Data final {
                    public:
                        /** Default constructor explicitly deleted. */
                        Data() = delete;
                        Data(std::vector<SharedMember<Member>> &&members, bool write, bool locked = false)
                            : members(std::forward<std::vector<SharedMember<Member>>>(members)), write(write), locked(locked) {}
                        const std::vector<SharedMember<Member>> members;
                        bool write;
                        bool locked;
                        bool fake;
                };

                std::shared_ptr<Data> data;
        };

        /** Obtains a read lock for the current object, returns a Member::Lock object.  This is
         * equivalent to calling the other readLock methods with an empty container.
         *
         * \sa readLock(const Container&)
         */
        Lock readLock() const {
            if (maxThreads() == 0) return Member::Lock(false); // Fake lock
            return Member::Lock(false, sharedSelf());
        }

        /** Obtains a read lock for the current object *plus* all the objects passed in via the
         * given container.  This will block until a read lock can be obtained on all objects.
         *
         * This method is designed to be deadlock safe: it will not block while holding any lock; if
         * unable to obtain a lock on one of the objects, it will release any other held locks
         * before blocking on the unobtainable lock.
         *
         * Because the fundamental mutex used for locking is recursive, it is safe to call this in
         * such a way that objects are locked multiple times.
         *
         * The lock will be released automatically when the returned object is destroyed, typically
         * by going out of scope.  It can also be explicitly controlled.
         *
         * \param plus any iterable object containing SharedMember<T> objects
         *
         * \sa Member::Lock
         */
        template <class Container>
        Lock readLock(const Container &plus,
                typename std::enable_if<
                    std::is_base_of<Member, typename decltype(std::declval<Container>().begin())::value_type::member_type>::value
                >::type* = 0) const {
            if (maxThreads() == 0) return Member::Lock(false); // Fake lock
            std::vector<SharedMember<Member>> members;
            members.push_back(sharedSelf());
            members.insert(members.end(), plus.begin(), plus.end());
            return Member::Lock(false, std::move(members));
        }

        /** Obtains a read lock for the current objects *plus* the all the SharedMember<T> values of
         * the provided map-like container.
         *
         * \param plus any iterable object containing std::pair<K, SharedMember<T>> objects.
         */
        template <class Container>
        Lock readLock(const Container &plus,
                typename std::enable_if<
                    std::is_base_of<Member, typename decltype(std::declval<Container>().begin())::value_type::second_type::member_type>::value
                    >::type* = 0) const {
            if (maxThreads() == 0) return Member::Lock(false); // Fake lock
            std::vector<SharedMember<Member>> members;
            members.push_back(sharedSelf());
            for (auto &p : plus)
                members.push_back(p.second);
            return Member::Lock(false, std::move(members));
        }

        /** Obtains an exclusive read/write lock for the current object, returning a Member::Lock
         * object.  The write lock lasts until the returned Lock object is destroyed (typically by
         * going out of scope), or is explicitly unlocked via Lock methods.
         *
         * You can safely (i.e. without deadlocking) obtain multiple write locks on the same object
         * from the same thread, but attempting to obtain a write lock over top of a read lock in
         * the same thread will deadlock: don't do that.  You may also safely obtain read locks over
         * top of write locks so long as no write lock is obtained until all the thread's read locks
         * are released.
         *
         * This held lock will always be an exclusive lock: no other read locks or write locks will
         * be active.
         *
         * The lock provided is advisory: it is still possible for a thread without a write lock to
         * invoke changes on the locked object, but such should be considered a serious error.
         */
        Lock writeLock() {
            if (maxThreads() == 0) return Member::Lock(true); // Fake lock
            auto s = sharedSelf();
            auto l = Member::Lock(true, sharedSelf());
            return l;
        }

        /** Obtains a write lock for the current object *plus* all the objects passed in.  This will
         * block until a write lock can be obtained on all objects.
         *
         * Like readLock, this method is deadlock safe if used properly: it will not block
         * waiting for a lock while holding any other locks, and, because of the use of a recursive
         * mutex, will not deadlock when an object is included multiple times in the list of objects
         * to lock.
         *
         * Overlapping write locks on the same object within the same thread are allowed, but write
         * locks on objects that are read-locked in the same thread will cause a deadlock.  You may
         * overlap the other way (i.e. obtaining a read lock on a write-locked object), so long as
         * you never try to obtain a write lock over top of the read lock.
         *
         * It is possible to switch between write and read locks: see Member::Lock for details.
         *
         * The lock will be released when the returned object is destroyed, typically by going out
         * of scope.
         */
        template <class Container>
        Lock writeLock(const Container &plus,
                typename std::enable_if<
                    std::is_base_of<Member, typename decltype(std::declval<Container>().begin())::value_type::member_type>::value
                >::type* = 0) const {
            if (maxThreads() == 0) return Member::Lock(true); // Fake lock
            std::vector<SharedMember<Member>> members;
            members.push_back(sharedSelf());
            members.insert(members.end(), plus.begin(), plus.end());
            return Member::Lock(true, std::move(members));
        }

        /** Obtains a read lock for the current objects *plus* the all the SharedMember<T> values of
         * the provided map-like container.
         *
         * \param plus any iterable object containing std::pair<K, SharedMember<T>> objects.
         */
        template <class Container>
        Lock writeLock(const Container &plus,
                typename std::enable_if<
                    std::is_base_of<Member, typename decltype(std::declval<Container>().begin())::value_type::second_type::member_type>::value
                    >::type* = 0) const {
            if (maxThreads() == 0) return Member::Lock(true); // Fake lock
            std::vector<SharedMember<Member>> members;
            members.push_back(sharedSelf());
            for (auto &p : plus)
                members.push_back(p.second);
            return Member::Lock(true, std::move(members));
        }

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
            if (!dynamic_cast<B*>(obj.ptr().get())) throw std::invalid_argument(error);
        }

        /** Returns a SharedMember wrapper around the current object, obtained through the
         * Simulation object (so that the shared_ptr is properly shared with everything else that
         * has a SharedMember pointing to the same object).
         *
         * This returns a generic SharedMember<Member>, which is castable to SharedMember<O> where O
         * is the actual O subclass the object belongs to.
         *
         * This is deliberately not provided by Member to make Member an abstract class.
         */
        virtual SharedMember<Member> sharedSelf() const = 0;

        /** Returns the maximum number of threads in the simulation.  This is simply an alias for
         * simulation()->maxThreads().
         */
        unsigned long maxThreads() const;

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

        /** Called during Lock destruction to release the locks on the current object and
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
inline unsigned long Member::maxThreads() const {
    return simulation()->maxThreads();
}
}
