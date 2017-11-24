#pragma once
#include <eris/Simulation.hpp>
#include <eris/types.hpp>
#include <eris/noncopyable.hpp>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <set>
#include <condition_variable>
#include <type_traits>
#include <algorithm>
#include <string>
#include <ostream>

namespace eris {

/** Base class for "members" of a simulation: often goods, agents, markets, and optimizers.  This class
 * provides an id, a weak reference to the owning simulation object, a few utility methods, and lock
 * functionality for aiding thread-safety.
 *
 * This class is a base class for all objects belonging to an eris simulation: Goods, Agents,
 * and Markets (see those classes for details).  It is also used for other, generic members that are
 * none of the above such as optimization classes.
 */
class Member : private noncopyable {
public:
    /// Default constructor.
    Member() = default;

    virtual ~Member() = default;

    /** Returns the eris::id_t ID of this member.  An ID is assigned when the Member is created, and
     * is unique across all Member instances.
     */
    id_t id() const { return id_; }

    /** Returns true if this member belongs to a simulation, false otherwise.
     */
    bool hasSimulation() const;

    /** Creates and returns a shared pointer to the simulation this object belongs.  If the member
     * does not belong to a simulation, throws a no_simulation_error exception.  The optional `T`
     * parameter allows casting the shared pointer to a Simulation subclass.
     */
    template <typename T = Simulation>
    std::shared_ptr<T> simulation() const {
        auto shptr = simulation_.lock();
        if (!shptr)
            throw no_simulation_error();
        return as_sim_<T>(std::move(shptr));
    }

    /** Shortcut for `member.simulation()->agent<A>()` */
    template <class A = Agent> SharedMember<A> simAgent(MemberID aid) const {
        return simulation()->agent<A>(aid);
    }
    /** Shortcut for `member.simulation()->good<G>()` */
    template <class G = Good> SharedMember<G> simGood(MemberID gid) const {
        return simulation()->good<G>(gid);
    }
    /** Shortcut for `member.simulation()->market<M>()` */
    template <class M = Market> SharedMember<M> simMarket(MemberID mid) const {
        return simulation()->market<M>(mid);
    }
    /** Shortcut for `member.simulation()->other<O>()` */
    template <class O = Member> SharedMember<O> simOther(MemberID oid) const {
        return simulation()->other<O>(oid);
    }

    /// Shortcut for `member.simulation()->t()`, i.e. returns the current simulation time period.
    time_t simT() const { return simulation()->t(); }

    /** Records a dependency with the Simulation object.  This should not be called until after the
     * member has been added to a simulation, and is typically invoked in an overridden added()
     * method.
     *
     * This is simply an alias for Simulation::registerDependency; the two following statements are
     * exactly equivalent:
     *
     *     member->dependsOn(other_member);
     *     member->simulation()->registerDependency(member, other_member);
     *
     * As a result of the dependency, this object will be removed from the simulation if the
     * depended upon object is removed.
     *
     * \sa Simulation::registerDependency()
     * \sa dependsWeaklyOn()
     */
    void dependsOn(MemberID id);

    /** Records a dependency with the Simulation object.  This should not be called until after the
     * member has been added to a simulation, and is typically invoked in an overridden added()
     * method.
     *
     * This is simply an alias for Simulation::registerWeakDependency; the two following statements
     * are exactly equivalent:
     *
     *     member->dependsWeaklyOn(other_member);
     *     member->simulation()->registerWeakDependency(member, other_member);
     *
     * As a result of the weak dependency, this object's weakDepRemoved() method will be called if
     * the target member is removed from the simulation.
     *
     * \sa Simulation::registerWeakDependency()
     * \sa dependsOn()
     * \sa weakDepRemoved()
     */
    void dependsWeaklyOn(MemberID id);

private:
    using Mutex = std::shared_timed_mutex;

    /// Shared mutex used by Member::Lock to read (shared) and write (exclusive) locks.
    mutable Mutex mutex_;

    /// Locks a single member mutex with an exclusive or shared lock.
    void lock_(bool exclusive) { if (exclusive) mutex_.lock(); else mutex_.lock_shared(); }
    /// Unlocks a single member mutex with an existing exclusive or shared lock.
    void unlock_(bool exclusive) { if (exclusive) mutex_.unlock(); else mutex_.unlock_shared(); }
    /// Tries to obtain a exclusive or shared lock on a member mutex
    bool try_lock_(bool exclusive) { return exclusive ? mutex_.try_lock() : mutex_.try_lock_shared(); }

    /// The set type used by locks:
    using MemberSet = std::unordered_set<SharedMember<Member>>;

public:
    /** A RAII-style locking class for holding one or more simultaneous Member locks.  Locks are
     * established during object construction and released when the object is destroyed.  A Lock may
     * be copied, in which case all copies must be destroyed before the lock is automatically
     * released.
     *
     * This class satisfies the requirements of Lockable (that is, is has lock(), unlock(), and
     * try_lock() methods).
     *
     * When locking multiple objects at once, this class is designed to avoid deadlocks: it will not
     * block while waiting for a lock while it holds any open locks; instead if a lock cannot be
     * obtained, any previous locks are released while waiting for the locked object to become
     * unlocked.  Construction (or a lock() call) do not return until all required locks are held.
     *
     * There are provided public methods for manually releasing and re-obtaining the locks, and
     * methods for converting a read lock into a write lock and vice-versa.  These methods are
     * typically not needed (when object lifetime will suffice), but available for convenience for
     * more precise lock control.
     *
     * It is not recommended (or supported) to access a Lock object from multiple threads.
     *
     * If the simulation is not using threads at all (that is, maxThreads() is 0), the methods of
     * this class do nothing substantial: no actual locking operations are performed thus
     * eliminating locking overhead when locking is pointless.
     *
     * If you need to obtain multiple Member::Locks at once (for example, to hold a read lock on
     * some members and a write lock on others), you should first unlock all locks, then call use
     * std::lock() to reobtain locks on all of them.
     *
     * Implementation details:
     *
     * Every Member-derived object has a lock mutex that governs write access to the object.
     * Additionally, each Member has a readlocks variable (access to which requires locking the
     * mutex).  A write lock is thus a case of holding the mutex while readlocks is set to 0.  A
     * read lock is simply an object that has readlocks set to a value greater than 0.  Establishing
     * a read lock thus requires locking the mutex, incrementing the readlocks variable, then
     * releasing the mutex lock.
     *
     * To establishing a lock in both cases (read or write), we first try (in a non-blocking way) to
     * obtain mutex locks on all of the Member-derived objects to be locked.  If any of them fails
     * (either because locking the mutex would block, or because need a write lock but when we got
     * the mutex lock the object has readlocks > 0, and thus must wait for outstanding readlock(s)
     * to finish), we unlock any held mutexes then do a blocking lock on the failed mutex.  When we
     * finally get that lock, we restart the process.
     *
     * Once we get through the entire set of mutexes, and thus hold locks on all required objects,
     * if we want a write lock, we're finished.  If we want a read lock, we increment all the
     * objects' readlocks variable, then release all the mutex locks.
     *
     * Unlocking is easier: for a write lock, we simply release all the mutex locks.  For a read
     * lock, we have to briefly a obtain a mutex lock on the object, decrement its readlocks
     * variable, then release the mutex lock.  (Since a Lock object never holds a mutex lock while
     * waiting for something else, these blocking locks are guaranteed to be non-deadlocking).
     */
    class Lock final {
    public:
        /** Default constructor explicitly deleted. */
        Lock() = delete;
        /** Copy constructor.  Automatic lock release happens once all copies are destroyed.  Both
         * also share lock status: releasing one or converting one into a different type of lock
         * also releases or converts any copies.*/
        Lock(const Lock &l);
        /** Releases the held lock(s) (unless copies of the Lock are still around). */
        ~Lock();
        /** If the lock is currently a read lock (or is currently inactive), calling this converts
         * it to a write lock and establishes the lock.  If the lock is already a write lock, this
         * does nothing.  Since all copies of this Lock object share the lock status, they will
         * simultaneously become write locks.
         *
         * If the lock type is changed or the current lock is not established, this method may block
         * until the lock is obtained.
         *
         * If the optional `only_try` parameter is given and true, the write lock is established
         * only if it can be done without blocking.  If blocking would be required, the lock is left
         * unlocked (even if it was a previously active read lock) and false is returned.  Note that
         * even if it lock fails, the lock will still be changed to a write lock if it was
         * previously a read lock (that is, a subsequent `lock()` call will establish a write lock).
         *
         * \returns true if the write lock was established (or already active), false if `try_lock =
         * true` was given and the write lock could not be immediately obtained.
         */
        bool write(bool only_try = false);
        /** Converts the current lock into a read lock.  If the lock is currently an active write
         * lock, it is first released and then reacquired as a read lock.  If the lock in inactive,
         * it is acquired as a read lock.  If the lock is currently an active read lock, this does
         * nothing.
         *
         * If the lock type is changed or the current lock is not established, this method may block
         * until the lock is obtained.
         *
         * If the optional parameter `only_try` is provided and true, the lock is established only
         * if it is possible to do so without blocking.  If blocking would be required, the lock is
         * left unlocked and false is returned.  Note that even if the lock fails, the lock will
         * still be converted to a read lock if it was previously a write lock (that is, a
         * subsequent `lock()` call will establish a read lock).
         *
         * \returns true if the read lock was established (or already active), false if `try_lock =
         * true` was given and the read lock could not be immediately obtained.
         */
        bool read(bool only_try = false);
        /** Obtains the lock.  This is called automatically when the object is created and, if
         * needed, when switching lock types, but may also be called manually.  Note that this will
         * block if the lock is currently not available.
         *
         * \throws std::system_error with an error code of std::errc::resource_deadlock_would_occur
         * if the lock is already active.
         */
        void lock();
        /** Tries to obtain a lock.  If a lock cannot be obtained without blocking, returns false;
         * otherwise, if the lock is obtained, returns true.
         *
         * \throws std::system_error with an error code of std::errc::resource_deadlock_would_occur
         * if the lock is already active.
         */
        bool try_lock();
        /** Releases the held lock.  This is called automatically when the object is destroyed and
         * when switching lock types, but may also be called manually.
         *
         * \throws std::system_error with an error code of std::errc::operation_not_permitted if the
         * lock is not currently active.
         */
        void unlock();
        /** Returns true if this lock is currently a write lock, false if it is a read lock.  Note
         * that this does not distinguish between inactive and active locks.
         */
        bool isWrite() { return data->write; }
        /** Alias for `!isWrite()` */
        bool isRead() { return !isWrite(); }
        /** Returns true if this is a "fake" lock, that is, a lock on members in a simulation where
         * threading is disabled (and thus the actual locking mechanics are skipped).  Mainly
         * internal use.
         */
        bool isFake() { return data->members.empty(); }
        /** Returns true if this lock is currently active (i.e. actually a lock), false otherwise.
         * Note that this status is shared among all copies of a Lock object.
         */
        bool isLocked() { return data->locked; }

        /** Tries to add the given member to the current set of locked members.  If the member can
         * be added to the current lock without blocking (i.e. because there isn't a restricting
         * lock, or the current lock isn't currently locked at all), the member is added and true is
         * returned.  Otherwise the member is not added and false is returned.
         *
         * This method is mainly for internal use of `add()` but can be called directly if needed.
         */
        bool try_add(const SharedMember<Member> &member);

        /** Adds the given member to the current set of locked members.  If a lock on the given
         * member cannot be obtained immediately, all locks currently held by the Lock object are
         * released until a lock on all objects (current plus new) can be obtained.  If the current
         * lock already contains the given member, this does nothing.
         *
         * If the object is currently not locked, this takes out no lock, but adds the given member
         * to the set of members that will be locked when lock() is called.
         */
        void add(const SharedMember<Member> &member);

        /** Container-accepting version of add(). */
        template <class Container,
            std::enable_if_t<std::is_base_of<Member, typename Container::value_type::member_type>::value, int> = 0>
        void add(const Container &members) {
            bool add_failed = false;
            for (auto &mem : members) {
                if (!try_add(mem)) { // Adding the member would block
                    // Release the lock, add this (and then any remaining members), then relock at the end
                    add_failed = true;
                    unlock();
                    try_add(mem);
                }
            }
            if (add_failed)
                lock(); // Relock with everything
        }

        /** Transfers the locked members of `from` into the called lock.  After the call, the
         * calling object will own the locks of its current members and all the members of `from`,
         * while `from` (including copies of the lock object) will be an empty lock.
         *
         * Both locks must be of the same type (read or write), and in the same state (locked or
         * released); otherwise a Member::Lock::mismatch_error exception is thrown.
         */
        void transfer(Lock &from);

        /** Exception class thrown by transfer(Lock) if the source and destination locks are in
         * different read/write and/or locked/released states.
         */
        class mismatch_error : public std::runtime_error {
            public:
                mismatch_error() : std::runtime_error("Lock transfer() failed: recipient and source have different lock states") {}
        };

        /** Removes the given member from the current lock, transferring it to a new lock of the
         * same type and status as the current lock without releasing the lock (if active).  Thus
         * this method cannot block.  If the returned lock is not stored, it expires immediately and
         * so this also serves to remove and release the lock on given members.
         *
         * \throws std::out_of_range if the lock doesn't contain the given member.
         */
        Lock remove(const SharedMember<Member> &member);

        /** RAII class returned by supplement().  The class is move constructible but not copyable
         * or move assignable; it adds the given member during construction, and removes it during
         * destruction.
         */
        class Supplemental final : private eris::noncopyable {
        public:
            /// Not default constructible
            Supplemental() = delete;
            /// Constructs a supplemental lock that adds `member` to `lock` when constructed
            Supplemental(Lock &lock, const SharedMember<Member> &member);
            /// Constructs a supplemental lock that adds a container of `members` to `lock` when constructed
            template <class Container, typename = typename
                std::enable_if<std::is_base_of<Member, typename Container::value_type::member_type>::value, Lock>::type>
            Supplemental(Lock &lock, const Container &members) : lock_{lock}, members_(members.begin(), members.end()) {
                lock_.add(members_);
            }
            /// Destructor: removes member from lock
            ~Supplemental();
            /** Move constructor */
            Supplemental(Supplemental &&s) : lock_{s.lock_}, members_{std::move(s.members_)} {}

        private:
            Lock &lock_;
            std::list<SharedMember<Member>> members_;
        };

        /** This is an RAII version of add(): it adds the given member to the lock (releasing and
         * blocking, as per add()), but returns a Supplemental object that automatically removes the
         * member from the lock when destroyed.  This thus allows usage such as:
         *
         *     {
         *         auto extra_lock = lock.supplement(member);
         *         // ... this code has a lock on all of `lock`s members plus member
         *     }
         *    // extra_lock is destroyed: `member` is no longer part of `lock`.
         *
         * as an alternative to:
         *
         *     {
         *         lock.add(member);
         *         // ...
         *         lock.remove(member);
         *     }
         *
         * You must ensure that the returned object does not persist beyond the lifetime of the lock
         * it is based upon.
         */
        [[gnu::warn_unused_result]]
        Supplemental supplement(const SharedMember<Member> &member);

        /** Container-accepting version of supplement() */
        template <class Container,
            std::enable_if_t<std::is_base_of<Member, typename Container::value_type::member_type>::value, int> = 0>
        [[gnu::warn_unused_result]]
        Supplemental supplement(const Container &members) {
            return Supplemental(*this, members);
        }

        /** Removes the given members from the current lock, transferring them to a new lock of the
         * same type and status as the current lock without releasing the locks on those members (if
         * active).  Since this method does not take out any new locks, it cannot block.  If the
         * returned lock is not stored, it expires immediately, thus releasing the locks held on the
         * given objects.  If the given container is empty, this will return a fake lock (i.e. a
         * lock with no members).
         *
         * If a requested member is in the lock multiple times, only one instance of the member is
         * removed per copy of the member in the passed-in list.  As a result, it is possible that
         * the original lock still has a lock on passed-in members if the given member was added
         * multiple times.
         *
         * \throws std::out_of_range if the lock doesn't contain one or more of the given members.
         */
        template <class Container,
            std::enable_if_t<std::is_base_of<Member, typename Container::value_type::member_type>::value, int> = 0>
        Lock remove(const Container &members) {
            if (members.empty()) return Lock(isWrite(), isLocked()); // Fake lock

            MemberSet new_lock_members;
            for (auto &mem : members) {
                auto found = data->members.find(mem);
                if (found == data->members.end())
                    throw std::out_of_range("Member passed to Lock.remove() is not contained in the lock");

                new_lock_members.insert(*found);
                data->members.erase(found);
            }
            return Member::Lock(isWrite(), isLocked(), std::move(new_lock_members));
        }

    private:
        /** Constructs a fake lock.  This is equivalent to create a Lock with no members.  This is
         * used by Member.readLock and .writeLock when the simulation doesn't use threading so
         * avoids all the actual lock code, or when attempting to remove an empty list of members
         * from a lock via remove().  A fake lock still has read/write and locked/released states
         * which are maintained appropriately through calls to lock()/unlock()/write()/read(),
         * though all locking/releasing action does nothing other than keeping track of the state.
         */
        explicit Lock(bool write, bool locked = true);

        /** Creates a lock that applies to a set of members. Calls lock() (which calls read() or
         * write()) before returning. */
        Lock(bool write, MemberSet &&members);

        /** Creates a lock that applies to a set of members, initially in the given lock status.
         * Note that this does *not* establish a lock, even if `lock` is true: this method is
         * primarily intended for use by remove() to split a Lock into multiple Locks without
         * requiring an intermediate release and relocking.
         */
        Lock(bool write, bool locked, MemberSet &&members);

        /** Obtains a lock on all members.  If `write` is true, all locks will be exclusive;
         * otherwise all locks will be shared.  This method blocks until a mutex is held on all
         * members, but never blocks while holding any mutex lock.  When this method returns, a
         * mutex lock is held on every member.
         *
         * The optional `only_try` parameter changes the behaviour: if provided and true, the mutex
         * lock is obtained only if it can be done without blocking.  `true` is returned if the
         * mutex locks were established, false if blocking is required.  When `only_try` is false
         * (the default if omitted), this method always returns true.
         *
         * This code is the core simultaneous locking code used by read() and write() to establish a
         * lock on all members.
         */
        bool lock_all_(bool write, bool only_try = false);

        friend class Member;

        class Data final {
            public:
                /** Default constructor explicitly deleted. */
                Data() = delete;
                Data(MemberSet &&mbrs, bool wrt, bool lckd = false)
                    : members{std::move(mbrs)}, write{wrt}, locked{lckd}
                {}
                MemberSet members;
                bool write;
                bool locked;
        };

        std::shared_ptr<Data> data;
    };

    /** Obtains a read lock for the current object *plus* all the objects passed in; provided
     * objects can either be provided as singletons, or as an stl or stl-compatible container.
     * Multiple objects and/or containers are permitted.  This will block until a read lock can be
     * obtained on all objects.
     *
     * This method is designed to be deadlock safe: it will not block while holding any lock; if
     * unable to obtain a lock on one of the objects, it will release any other held locks before
     * blocking on the unobtainable lock.
     *
     * Because the fundamental mutex used for locking is recursive, it is safe to call this in such
     * a way that objects are locked multiple times.
     *
     * The lock will be released automatically when the returned object is destroyed, typically by
     * going out of scope.  It can also be explicitly controlled.
     *
     * If the calling object is not a Simulation member, the lock won't be enforced on the caller.
     * (In such a case there is no reliable way to obtain a SharedMember wrapper around the caller).
     * This is unlikely to be an issue as it only typically comes up during destruction phases.
     *
     * \param plus any number of SharedMember<T> objects or iterable containers containing
     * SharedMember<T> objects
     *
     * \sa Member::Lock
     */
    template <typename... Args>
    [[gnu::warn_unused_result]] Lock readLock(Args &&...more) const {
        return rwLock_(false, std::forward<Args>(more)...);
    }

    /** Obtains a write lock for the current object *plus* all the objects passed in; provided
     * objects can either be provided as singletons, or as an stl or stl-compatible container.
     * Multiple objects and/or containers are permitted.  This will block until a write lock can be
     * obtained on all objects.
     *
     * Like readLock, this method is deadlock safe if used properly: it will not block waiting for a
     * lock while holding any other locks, and, because of the use of a recursive mutex, will not
     * deadlock when an object is included multiple times in the list of objects to lock.
     *
     * Overlapping write locks on the same object within the same thread are allowed, but write
     * locks on objects that are read-locked in the same thread will cause a deadlock.  You may
     * overlap the other way (i.e. obtaining a read lock on a write-locked object), so long as you
     * never try to obtain a write lock over top of the read lock.
     *
     * It is possible to switch between write and read locks: see Member::Lock for details.
     *
     * The lock will be released when the returned object is destroyed, typically by going out of
     * scope.
     *
     * If the calling object is not a Simulation member, the lock won't be enforced on the caller.
     * (In such a case there is no reliable way to obtain a SharedMember wrapper around the caller).
     * This is unlikely to be an issue as it only typically comes up during destruction phases.
     *
     * \param plus any number of SharedMember<T> objects or iterable containers containing
     * SharedMember<T> objects
     *
     * \sa Member::Lock
     */
    template <typename... Args>
    [[gnu::warn_unused_result]] Lock writeLock(Args &&...more) const {
        return rwLock_(true, std::forward<Args>(more)...);
    }

    /** Error class throw when attempting to perform a member action requiring a simulation when the
     * member is not currently a member of a simulation.
     */
    class no_simulation_error : public std::runtime_error {
        public:
            no_simulation_error() : std::runtime_error("Action requires a simulation but the member does not belong to a simulation") {}
    };

protected:
    /** Called (by Simulation) to store a weak pointer to the simulation this member belongs to.
     * This is called once when the Member is added to a simulation, and again (with a null shared
     * pointer) when the Member is removed from the Simulation.
     */
    void simulation(const std::shared_ptr<Simulation> &sim);
    friend class eris::Simulation;

    /** Virtual method called just after the member is added to a Simulation object.  The default
     * implementation does nothing.  This method is typically used to record a dependency in the
     * simulation, but can also do initial setup.
     */
    virtual void added();

    /** Virtual method called just after the member has been removed from a Simulation object.  The
     * default implementation does nothing.  The simulation() and id() methods are still available,
     * but the simulation no longer references this Member.
     *
     * Note that any registered dependencies may not exist (in particular when the removal is
     * occuring as the result of a cascading dependency removal).  In other words, if this Member
     * has registered a dependency on B, when B is removed, this Member will also be removed *after*
     * B has been removed from the Simulation.
     */
    virtual void removed();

    /** Helper method to ensure that the passed-in SharedMember is a subclass of the templated
     * class.  If not, this throws an invalid_argument exception with the given message.
     */
    template<class B, class C>
    void requireInstanceOf(const SharedMember<C> &obj, const std::string &error) {
        if (!dynamic_cast<B*>(obj.ptr().get())) throw std::invalid_argument(error);
    }

    /** Called when a weak dependent of this object is removed from the simulation.  The default
     * implementation does nothing.
     *
     * This method is called after the removed member has had its own `removed()` method called, and
     * after any non-weak dependencies of the removed member have themselves been removed.
     *
     * \param removed is the dependency that was just removed
     * \sa dependsWeaklyOn()
     * \sa Simulation::registerWeakDependency()
     */
    virtual void weakDepRemoved(SharedMember<Member> removed);

    /** Returns a SharedMember wrapper around the current object, obtained through the Simulation
     * object (so that the shared_ptr is properly shared with everything else that has a
     * SharedMember pointing to the same object).
     *
     * This returns a generic SharedMember<Member>, which is castable to SharedMember<O> where O is
     * the actual O subclass the object belongs to.
     *
     * The default implementation returns a member from the simulation's "other" set; Agent, Good,
     * and Market have overrides to return from the other appropriate sets.
     */
    virtual SharedMember<Member> sharedSelf() const;

    /** Returns the maximum number of threads in the simulation.  This is simply an alias for
     * simulation()->maxThreads().
     */
    unsigned long maxThreads() const { return simulation()->maxThreads(); }

    /// Obtains a string representation of this member, primarily for debugging.  The default is
    /// `Member[n]` where `n` is the id() of the agent, but subclasses may override.
    virtual explicit operator std::string() const;

    /// Converts the object to a std::string when sent to an output stream
    friend std::ostream& operator << (std::ostream &os, const Member &m);

private:
    // The global id counter
    static std::atomic<id_t> next_id_;

    // The unique id
    id_t id_{next_id_++};

    /** Stores a weak pointer to the simulation this Member belongs to. */
    std::weak_ptr<eris::Simulation> simulation_;

    /// Generic version: uses simulation to cast itself to the desired type.  A specialization for
    /// T = Simulation, below, avoids the extra shared_ptr and cast when T = Simulation.
    template <typename T> static std::shared_ptr<T> as_sim_(std::shared_ptr<Simulation> &&sim) {
        return sim->as<T>();
    }

    /// Helper method doing all the grunt work of readLock/writeLock.
    template <typename... Args>
    Lock rwLock_(const bool &write, Args &&...args) const {
        const bool has_sim = hasSimulation();
        if (has_sim and maxThreads() == 0) return Member::Lock(write); // Fake lock
        MemberSet members;
        if (has_sim)
            members.insert(sharedSelf());
        member_zip_(members, std::forward<Args>(args)...);

        // members could be empty if we tried to get a writeLock on just an object that isn't in the
        // simulation (e.g. during some destructions)
        if (members.empty()) return Member::Lock(write);

        return Member::Lock(write, std::move(members));
    }

    /** Sticks the passed-in SharedMember<T> objects into the passed-in MemberSet */
    template <typename... Args>
    void member_zip_(MemberSet &zip, SharedMember<Member> add, Args &&...more) const {
        zip.insert(add);
        member_zip_(zip, std::forward<Args>(more)...);
    }

    /// Copies a container's contents into the passed-in MemberSet */
    template <typename Container, typename... Args,
             std::enable_if_t<std::is_base_of<Member, typename Container::value_type::member_type>::value, int> = 0>
    void member_zip_(MemberSet &zip, const Container &plus, Args &&...args) const {
        zip.insert(plus.begin(), plus.end());
        member_zip_(zip, std::forward<Args>(args)...);
    }

    /// Recursion terminator:
    void member_zip_(MemberSet &) const {}
};

// Non-casting specialization that avoids an extra shared_ptr creation + static cast:
template <> inline std::shared_ptr<Simulation> Member::as_sim_<Simulation>(std::shared_ptr<Simulation> &&sim) {
    return sim;
}

}
