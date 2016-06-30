#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <eris/noncopyable.hpp>
#include <cstddef>
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <list>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/lock_types.hpp>

/// Base namespace containing all eris classes.
namespace eris {

// Forward declarations
class Member;
class Agent;
class Good;
class Market;

/** This class is at the centre of an Eris economy model; it keeps track of all of the agents
 * currently in the economy, all of the goods currently available in the economy, and the
 * interaction mechanisms (e.g. markets).  Note that all of these can change from one period to the
 * next.  It's also responsible for dispatching interactions (e.g. letting markets operate) and any
 * iteration-sensitive agent events (e.g. aging/dying/etc.).
 *
 * In short, this is the central piece of the Eris framework that dictates how all the other pieces
 * interact.
 */
class Simulation final : public std::enable_shared_from_this<Simulation>, private noncopyable {
    public:
        /** Creates a new Simulation and returns a shared_ptr to it.  This is the only public
         * interface to creating a Simulation.
         */
        static std::shared_ptr<Simulation> create();

        /// Destructor.  When destruction occurs, any outstanding threads are killed and rejoined.
        virtual ~Simulation();

        /// Alias for a map of eris_id_t to SharedMember<A> of arbitrary type A
        template <class T> using MemberMap = std::unordered_map<eris_id_t, SharedMember<T>>;
        /// typedef for the map of id's to the set of dependent members
        typedef std::unordered_map<eris_id_t, std::unordered_set<eris_id_t>> DepMap;

        /** Wrapper around std::enable_if that defines a typedef `type` (defaulting to
         * SharedMember<Derived>) only if `Derived` is a `Base` member type.  `Base` must itself be
         * a class derived from `Member`.  If `Base` is `Member` itself, this checks for "other"
         * member types--that is, it additionally requires that `Derived` is *not* derived from
         * `Agent`, `Good`, or `Market` (despite those being themselves derived from `Member`); for
         * other `Base` values it simply requires that `Derived` is actually derived from `Base`.
         */
        template <class Base, class Derived, class T = SharedMember<Derived>>
        struct enable_if_member : public std::enable_if<
            std::is_base_of<Member, Base>::value and
            std::is_base_of<Base, Derived>::value and
            (not std::is_same<Base, Member>::value or
             not ( // Base == Member, which means we only want to allow "other" member types:
                 std::is_base_of<Agent, Derived>::value or
                 std::is_base_of<Good, Derived>::value or
                 std::is_base_of<Market, Derived>::value)
            ),
            T> {};

#define ERIS_SIM_MEMBER_ACCESS(T, Base, NAME) \
        template <class T = Base> \
        typename enable_if_member<Base, T>::type \
        NAME(eris_id_t id) const { \
            std::lock_guard<std::recursive_mutex> lock(member_mutex_); \
            return SharedMember<T>(NAME##s_.at(id)); \
        }

        /** Accesses an agent given the agent's eris_id_t.  Templated to allow conversion to
         * a SharedMember of the given Agent subclass; defaults to Agent.
         */
        ERIS_SIM_MEMBER_ACCESS(A, Agent, agent) // agent(eris_id_t)

        /** Accesses a good given the good's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Good subclass; defaults to Good.
         */
        ERIS_SIM_MEMBER_ACCESS(G, Good, good) // good(eris_id_t)

        /** Accesses a market given the market's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Market subclass; defaults to Market.
         */
        ERIS_SIM_MEMBER_ACCESS(M, Market, market) // market(eris_id_t)

        /** Accesses a non-agent/good/market member that has been added to this simulation.  This is
         * typically an optimization object.
         */
        ERIS_SIM_MEMBER_ACCESS(O, Member, other) // other(eris_id_t)

#undef ERIS_SIM_MEMBER_ACCESS

        /** Returns true if the simulation has an agent with the given id, false otherwise. */
        bool hasAgent(eris_id_t id) const { std::lock_guard<std::recursive_mutex> lock(member_mutex_); return agents_.count(id) > 0; }
        /** Returns true if the simulation has a good with the given id, false otherwise. */
        bool hasGood(eris_id_t id) const { std::lock_guard<std::recursive_mutex> lock(member_mutex_); return goods_.count(id) > 0; }
        /** Returns true if the simulation has a market with the given id, false otherwise. */
        bool hasMarket(eris_id_t id) const { std::lock_guard<std::recursive_mutex> lock(member_mutex_); return markets_.count(id) > 0; }
        /** Returns true if the simulation has a non-agent/good/market member with the given id,
         * false otherwise. */
        bool hasOther(eris_id_t id) const { std::lock_guard<std::recursive_mutex> lock(member_mutex_); return others_.count(id) > 0; }

        /** Constructs a new T object, forwarding any given arguments Args to the T constructor, and
         * adds the new member to the simulation (but see below).  T must be a subclass of Member;
         * if it is also a subclass of Agent, Good, or Market it will be treated as the appropriate
         * type; otherwise it is treated as an "other" member.
         *
         * If the new object is spawned during an optimizer stage, adding the member to the
         * simulation is deferred until the current stage or stage priority level finishes; thus it
         * is not guaranteed that a spawned member actually belongs to the simulation immediately:
         * member spawning performed in optimizers should take this into account.  Deferred
         * insertion will be performed in the same order as the calls to spawn().
         *
         * Example:
         *     auto money = sim->spawn<Good::Continuous>("money");
         *     auto good = sim->spawn<Good::Continuous>("x");
         *     auto market = sim->spawn<Bertrand>(Bundle(good, 1), Bundle(money, 1));
         */
        template <class T, typename... Args>
        typename std::enable_if<std::is_base_of<Member, T>::value, SharedMember<T>>::type
        spawn(Args&&... args) {
            auto member_ptr = std::make_shared<T>(std::forward<Args>(args)...);
            return SharedMember<T>(add(member_ptr));
        }

        /** Adds a Member-derived object to the simulation, via shared pointer.  The given member
         * must not already belong to a simulation.  This method is primarily intended for use when
         * the templated-version of spawn cannot be used; creating objects with spawn() is generally
         * preferred.
         *
         * \throws std::logic_error if the member already belongs to a simulation
         */
        SharedMember<Member> add(std::shared_ptr<Member> new_member);

        /** Removes the given member (and any dependencies) from this simulation.
         *
         * If the member has an optimizer registered at the current optimization stage and priority,
         * the member removal is deferred to the end of the current stage/priority iteration.
         * Deferred members are removed in the same order as the call to remove().
         *
         * \throws std::out_of_range if the given id does not belong to this simulation.  If called
         * during optimization, the exception may also be deferred (to the encompassing run()).
         */
        void remove(eris_id_t id);

#define ERIS_SIM_FILTER(T, BASE, WHICH) \
        template <class T = BASE> \
        typename enable_if_member<BASE, T, std::vector<SharedMember<T>>>::type \
        WHICH##s(const std::function<bool(const T &WHICH)> &filter = nullptr) const { \
            return genericFilter(WHICH##s_, filter); \
        }

#define ERIS_SIM_FILTER_COUNT(T, BASE, WHICH, METH_TYPE) \
        template <class T = BASE> \
        typename enable_if_member<BASE, T, size_t>::type \
        count##METH_TYPE##s(const std::function<bool(const T &WHICH)> &filter = nullptr) const { \
            return genericFilterCount(WHICH##s_, filter); \
        }

        /** Provides a vector of SharedMember<A>, optionally filtered to only include agents that
         * induce a true return from the provided filter function.
         *
         * The template class, A, provides a second sort of filtering: if it is provided (it must be
         * a class ultimately derived from Agent), only agents of type A will be returned.  If the
         * filter is also provided, only A objects will be passed to the filter and only returned if
         * the filter returns true.
         *
         * The results of class filtering are cached so long as the template filter class is not the
         * default (`Agent`) so that subsequent calls will not need to reperform extra work for
         * class filtering.  This helps considerably when searching for agents whose type is only a
         * small subset of the overall set of agents.  The cache is invalidated when any agent (of
         * any type) is added or removed.
         */
        ERIS_SIM_FILTER(A, Agent, agent) // agents()

        /** Provides a count of matching simulation agents.  Agents are filtered by class and/or
         * callable filter and the count of matching agents is returned.  This is equivalent to
         * agents<A>(filter).size(), but more efficient.
         *
         * Note that this method populates and uses the same cache as agents() when `A` is not the
         * default `Agent` class.
         */
        ERIS_SIM_FILTER_COUNT(A, Agent, agent, Agent) // countAgents()

        /** Provides a filtered vector of simulation goods.  This works just like agents(), but for
         * goods.
         *
         * \sa agents()
         */
        ERIS_SIM_FILTER(G, Good, good) // goods()

        /** Provides a count of matching simulation goods.  Goods are filtered by class and/or
         * callable filter and the count of matching goods is returned.  This is equivalent to
         * goods<G>(filter).size(), but more efficient.
         */
        ERIS_SIM_FILTER_COUNT(G, Good, good, Good) // countGoods()

        /** Provides a filtered vector of simulation markets.  This works just like agents(), but
         * for markets.
         *
         * \sa agents()
         */
        ERIS_SIM_FILTER(M, Market, market) // markets()

        /** Provides a count of matching simulation markets.  Markets are filtered by class and/or
         * callable filter and the count of matching markets is returned.  This is equivalent to
         * markets<G>(filter).size(), but more efficient.
         */
        ERIS_SIM_FILTER_COUNT(M, Market, market, Market) // countMarkets()

        /** Provides a filtered vector of non-agent/good/market simulation objects.  This works just like
         * agentFilter, but for non-agent/good/market members.
         *
         * \sa agentFilter
         */
        ERIS_SIM_FILTER(O, Member, other) // others()

        /** Provides a count of matching simulation non-agent/good/market members.  Members are
         * filtered by class and/or callable filter and the count of matching members is returned.
         * This is equivalent to members<G>(filter).size(), but more efficient.
         */
        ERIS_SIM_FILTER_COUNT(O, Member, other, Other) // countOthers()

#undef ERIS_SIM_FILTER
#undef ERIS_SIM_FILTER_COUNT

        /** Records already-stored member `depends_on` as a dependency of `member`.  If `depends_on`
         * is removed from the simulation, `member` will be automatically removed as well.
         *
         * Note that dependents are removed *after* removal of their dependencies.  That is, if A
         * depends on B, and B is removed, the B removal occurs *first*, followed by the A removal.
         */
        void registerDependency(eris_id_t member, eris_id_t depends_on);

        /** Records already-stored member `depends_on` is a weak dependency of `member`.  In
         * contrast to a non-weak dependency, the member is only notified of the removal of the
         * dependent, it is not removed itself.
         *
         * When `depends_on` is removed from the simulation, `member` will have its depRemoved()
         * method called with the just-removed (but still not destroyed) member.
         */
        void registerWeakDependency(eris_id_t member, eris_id_t depends_on);

        /** Sets the maximum number of threads to use for subsequent calls to run().  The default
         * value is 0 (which uses no threads at all; see below).  If this is lowered between calls
         * to run(), excess threads (if any) will be killed off at the beginning of the next run()
         * call; if raised, new threads will be allowed to spawn as needed: `maxThreads(100)` will
         * not create 100 threads unless there are at least 100 tasks to be performed
         * simultaneously.
         *
         * Specifying 0 kills off all threads and skips the threading code entirely, turning all
         * Member locking code into no-ops, thus avoiding the overhead threading imposes.  Note that
         * specifying 1 thread still enables the threading code, but with only one thread.  This is
         * probably not what you want (as it incurs some small overhead over not threading at all,
         * by specifying 0), and is predominantly provided for testing purposes.
         * 
         * Whether using threads improves performance or not depends on the model under
         * investigation: in a situation where each agent does significant calculations on its own,
         * performance will improve significantly.  On the other hand, if each agent requires
         * exclusively locking the same set of resources (for example, obtaining a write lock on all
         * markets), the performance advantage of threads will be negligible as threads spend most
         * of their time waiting for other threads to finish.
         *
         * Attempting to call this method during a call to run() is not permitted and will throw an
         * exception.
         *
         * @throws std::runtime_error if called during a run() call.
         */
        void maxThreads(unsigned long max_threads);

        /** Returns the maximum number of threads that can be used in the current run() call (if
         * called during run()) or in the next run() call (otherwise).  Returns 0 if threading is
         * disabled entirely.
         *
         * This is guaranteed not to change during a call to run().
         *
         * \sa maxThreads(unsigned long)
         */
        unsigned long maxThreads() { return max_threads_; }

        /** Runs one period of period of the simulation.  The following happens, in order:
         *
         * - Simulation time period (accessible by `t()`) is incremented.
         * - Inter-period optimization:
         *   - All inter-period optimizers have interBegin() called.
         *   - All inter-period optimizers have interOptimize() called.
         *   - All inter-period optimizers have interApply() called.
         *   - All inter-period optimizers have interAdvance() called.
         * - Intra-period optimization:
         *   - All intra-period optimizers have intraInitialize() called.
         *   - All intra-period optimizers have intraReset() called.
         *   - All intra-period optimizers have intraOptimize() methods called to calculate an
         *     optimal strategy (to be applied in intraApply()).
         *   - All intra-period optimizers have intraReoptimize() methods called.
         *     - If one or more of the intraReoptimize() methods returns true, intra-period
         *       optimization restarts at the `intraReset()` stage..  Short-circuiting does *not*
         *       occur: all intra-period optimizers will run, even if some have already indicated a
         *       reoptimization is needed.
         *   - If no intra-period optimizer requested a reoptimization,
         *     all intra-period optimizers have their intraApply() method called.
         *   - All intra-period optimizers have their intraFinish() method called to indicate the
         *     end of the period.
         *
         * \throws std::runtime_error if attempting to call run() recursively (i.e. during a run()
         * call).
         */
        void run();

        /** Returns the iteration number, where 1 is the first iteration.  This is incremented just
         * before inter-period optimizers run, and so, if called from inter-period optimizer code,
         * this will return the t value for the upcoming stage, not the most recent stage.
         *
         * This is initially 0 (until the first run() call).
         */
        eris_time_t t() const { return t_; }

        /** enum of the different stages of the simulation, primarily used for synchronizing threads.
         *
         * (The explicit ': int' is here because the stages are used internally as indicies)
         */
        enum class RunStage : int {
            idle, // between-period/initial thread state
            kill, // When a thread sees this, it checks thr_kill_, and if it is the current thread id, it finishes.
            kill_all, // When a thread sees this, it finishes.
            // Inter-period optimization stages:
            inter_Begin, inter_Optimize, inter_Apply, inter_Advance,
            // Intra-period optimization stages:
            intra_Initialize, intra_Reset, intra_Optimize, intra_Reoptimize, intra_Apply, intra_Finish
        };

        /** The first actual stage value; every value >= this and <= RunStage_LAST is an
         * optimization stage.  Values lower than this are special values.
         */
        static const RunStage RunStage_FIRST = RunStage::inter_Begin;
        /// The highest RunStage value
        static const RunStage RunStage_LAST = RunStage::intra_Finish;

        /** The current stage of the simulation.
         */
        RunStage runStage() const;

        /** Returns true if the simulation is currently in any of the intra-period optimization
         * stages, false otherwise.
         */
        bool runStageIntra() const;

        /** Returns true if the simulation is currently in any of the inter-period optimization
         * stages, false otherwise.
         */
        bool runStageInter() const;

        /** Returns true if the simulation is currently in an optimization stage (whether inter- or
         * intra-period optimization).  Specifically, this includes inter-optimize, intra-optimize,
         * intra-reoptimize, and intra-reset.
         */
        bool runStageOptimize() const;

        /** Obtains a shared lock that, when held, guarantees that a simulation stage is not in
         * progress.  This is designed for external code (e.g. GUI displays) to sychronize code,
         * ensuring that it does not run simultaneously with an active run call.  This lock is held
         * (exclusively!) during run().
         */
        boost::shared_lock<boost::shared_mutex> runLock();

        /** Tries to obtain a lock that, when held, guarantees that a simulation stage is not in
         * progress.  If the lock cannot be obtained (i.e. because something else already holds it),
         * this returns an unheld lock object.
         */
        boost::shared_lock<boost::shared_mutex> runLockTry();

        /** Contains the number of rounds of the intra-period optimizers in the previous run() call.
         * A round is defined by a intraReset() call, a set of intraOptimize() calls, and a set of
         * intraReoptimize() calls.  A multi-round optimization will only occur when there are
         * interopt::Reoptimize-implementing optimizers.
         */
        int intraopt_count = -1;

#ifdef ERIS_TESTS
        /** Exposes the internal dependency map structure for testing purposes.  This method is not
         * available normally (requires compilation with -DERIS_TESTS).
         */
        const DepMap __deps() { return depends_on_; }
        /** Exposes the internal weak dependency map structure for testing purposes.  This method is
         * not available normally (requires compilation with -DERIS_TESTS).
         */
        const DepMap __weakDeps() { return weak_dep_; }
#endif

    private:
        /* Simulation constructor.  Simulation objects should be constructed by calling
         * Simulation::create() instead of calling the constructor directly.
         */
        Simulation() = default;

        unsigned long max_threads_ = 0;
        eris_id_t id_next_ = 1;
        MemberMap<Agent> agents_;
        MemberMap<Good> goods_;
        MemberMap<Market> markets_;
        MemberMap<Member> others_;

        // insert() decides which of following insertAgent, insertGood, etc. methods to call and
        // calls it.  Called from the public spawn() method.
        void insert(const SharedMember<Member> &member);

        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        void insertOther(const SharedMember<Member> &other);

        // Internal remove() method that doesn't defer if currently running
        void removeNoDefer(eris_id_t id);

        // Processes any deferred member insertions/removals.  This is called at the end of each
        // stage (while the exclusive run lock is held).
        void processDeferredQueue();

        // Internal method to remove one of the various types.  Called by the public remove()
        // method, which figures out which type the removal request is for.
        void removeAgent(eris_id_t aid);
        void removeGood(eris_id_t gid);
        void removeMarket(eris_id_t mid);
        void removeOther(eris_id_t oid);

        // Determines which (if any) optimization interfaces the member implements, and records it
        // for the next optimization stage.
        void insertOptimizers(const SharedMember<Member> &member);

        // Undoes insertOptimizers; called by the removeAgent(), etc. methods, which should already
        // hold a lock on member_mutex_.
        void removeOptimizers(const SharedMember<Member> &member);

        // The method used by agents(), goods(), etc. to actually do the work
        template <class T, class B>
        std::vector<SharedMember<T>> genericFilter(const MemberMap<B> &map, const std::function<bool(const T &member)> &filter) const;
        // The method used by countAgents(), countGoods(), etc. to actually do the work
        template <class T, class B>
        size_t genericFilterCount(const MemberMap<B> &map, const std::function<bool(const T &member)> &filter) const;
        // Method used by both of the above to access/create a class filter cache
        template <class T, class B>
        const std::vector<SharedMember<Member>>* genericFilterCache(const MemberMap<B> &map) const;

        // Typical element: filter_cache_[Agent][PriceFirm] = vector of SharedMember<Member>s which are really PriceFirms.
        // Mutable because these are built up in const methods, but are only caches and so don't
        // really change anything.
        mutable std::unordered_map<std::type_index, std::unordered_map<std::type_index,
                std::vector<SharedMember<Member>>>> filter_cache_;

        // Invalidate the filter cache for objects of type B, which should be one of the base Agent,
        // Good, or Market classes, or Member, which invalidates the "other" filter cache.

        template <class B>
        typename std::enable_if<
            std::is_same<B, Member>::value or std::is_same<B, Agent>::value or std::is_same<B, Good>::value or std::is_same<B, Market>::value,
            void
        >::type
        invalidateCache() {
            std::lock_guard<std::recursive_mutex> lock(member_mutex_);
            filter_cache_.erase(std::type_index(typeid(B)));
        }

        DepMap depends_on_, weak_dep_;

        // Removes hard dependents
        void removeDeps(eris_id_t member);

        // Notifies weak dependents
        void notifyWeakDeps(SharedMember<Member> member, eris_id_t old_id);

        // Tracks the iteration number, can be accessed via t().
        eris_time_t t_ = 0;

        /* Threading variables */

        // Protects member access/updates
        mutable std::recursive_mutex member_mutex_;

        // Mutex held exclusively during a run which is available for outside threads to ensure
        // operation not during an active stage.  See runLock() and runLockTry()
        boost::shared_mutex run_mutex_;

        // Pool of threads we can use
        std::vector<std::thread> thr_pool_;

        // The current optimizer stage
        RunStage stage_{RunStage::idle};

        // The current optimizer stage priority
        double stage_priority_;

        // The Members implementing each optimization stage.  vector indices are RunStage values;
        // map indices are priorities.
        std::vector<std::map<double, std::unordered_set<SharedMember<Member>>>> optimizers_{1 + (int) RunStage_LAST};

        // The maximum number of optimizers that can be run simultaneously; this is simply the
        // size of the largest set in optimizers_.  If negative, the value needs to be recalculated.
        long optimizers_plurality_ = -1;

        // The thread to quit when a kill stage is signalled
        std::atomic<std::thread::id> thr_kill_;

        // An iterator and past-the-end iterator through the currently running set of optimizers
        typename std::unordered_set<SharedMember<Member>>::iterator opt_iterator_, opt_iterator_end_;
        // Mutex controlling access to opt_iterator_
        std::mutex opt_iterator_mutex_;

        // The number of threads finished the current stage; when this reaches the size of
        // thr_pool_, the stage is finished.
        unsigned int thr_running_ = 0;

        // Will be set the false at the beginning of an intraopt round.  If a postOptimize returns
        // true (to restart the round), this will end up as true again; if still false at the end of
        // the postOptimize stage, the intraopt stage ends, otherwise it restarts.
        std::atomic_bool thr_redo_intra_{true};

        // CV for signalling a change in stage from master to workers
        std::condition_variable thr_cv_stage_;
        // mutex for the stage CV and for threads waiting for changes in stage_/stage_priority_
        std::mutex stage_mutex_;

        // CV for signalling the master that a thread has finished
        std::condition_variable thr_cv_done_;
        // mutex for the done CV and for threads signalling that they have finished the current
        // stage at the current priority level
        std::mutex done_mutex_;

        // List of members with deferred insertion to be inserted at the end of the current
        // stage/priority.
        std::list<SharedMember<Member>> deferred_insert_;
        // List of ids with deferred removal to be removed at the end of the current stage/priority
        std::list<eris_id_t> deferred_remove_;
        // Mutex governing the two above variables
        std::mutex deferred_mutex_;

        // Sets up the thread and/or shared queues for the appropriate stage.  This starts up
        // threads if needed (up to `max` or the number of members, whichever is smaller), sets
        // stage_, signals the threads to start, then wait for all threads to have signalled that
        // they are finished with the current stage.
        void thr_stage(const RunStage &stage);

        // Called at the beginning of run() to start up needed threads or kill off excess threads.
        //
        // Excess threads are those which exceed maxThreads().
        //
        // Needed threads is the smaller of maxThreads() and maximum simultaneous jobs, which is the
        // maximum of the number of optimizers for each optimizer type.
        //
        // Note that those numbers two values don't necessarily coincide.
        //
        // If this method changes the number of threads, this also takes care to invalidate queue
        // caches as needed so that appropriate thread reallocations will occur.
        //
        // This is called at the beginning of run(); stage_mutex_ should already be locked.
        void thr_thread_pool();

        // The main thread loop; runs until it sees a RunStage::kill with thr_kill_ set to the
        // thread's id.
        void thr_loop();

        // Called to process the current queue of waiting optimization objects.  When threading is
        // enabled, this is called simultaneously in each worker thread to process the queue in
        // parallel.
        template <class Opt>
        void thr_work(const std::function<void(Opt&)> &work);

        // Used to signal that the stage at the given priority level has finished.  After
        // signalling, this calls thr_wait to wait until the stage or priority change to something
        // other than the current values.
        void thr_stage_finished(const RunStage &curr_stage, double curr_priority);

        // Waits until stage or stage priority changes to something other than the passed-in values.
        void thr_wait(const RunStage &not_stage, double not_priority);
};

template <class T, class B>
const std::vector<SharedMember<Member>>* Simulation::genericFilterCache(
        const MemberMap<B>& map
        ) const {
    const auto &B_t = typeid(B), &T_t = typeid(T);

    // If T equals B we aren't class filtering at all, so there is no cache to build: eligible
    // elements are the enter set.
    if (B_t == T_t) return nullptr;

    const std::type_index B_h{B_t}, T_h{T_t};

    if (filter_cache_[B_h].count(T_h) == 0) {
        // Cache miss: build the [B_h][T_h] cache
        std::vector<SharedMember<Member>> cache;
        for (auto &m : map) {
            if (dynamic_cast<T*>(m.second.get())) {
                // The cast succeeded, so this member is a T
                cache.push_back(m.second);
            }
        }

        filter_cache_[B_h].emplace(std::type_index{T_h}, std::move(cache));
    }
    return &filter_cache_[B_h][T_h];
}


// Generic version of the various public ...s() methods that does the actual work.
template <class T, class B>
std::vector<SharedMember<T>> Simulation::genericFilter(
        const MemberMap<B>& map,
        const std::function<bool(const T &member)> &filter) const {

    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    auto *cache = genericFilterCache<T>(map); // Returns nullptr if not class filtering

    std::vector<SharedMember<T>> matched;
    if (cache) { // Class filtering
        for (auto &member : *cache) {
            SharedMember<T> recast(member);
            if (not filter or filter(recast))
                matched.push_back(std::move(recast));
        }
    }
    else { // Not class filtering, but possibly lambda filtering
        for (auto &m : map) {
            if (T* mem = dynamic_cast<T*>(m.second.get())) {
                // The cast succeeded, so this Member is also a T
                if (not filter or filter(*mem))
                    matched.push_back(SharedMember<T>{m.second});
            }
        }
    }
    return matched;
}

// Same as the above, but only gets a count.
template <class T, class B>
size_t Simulation::genericFilterCount(
        const MemberMap<B>& map,
        const std::function<bool(const T &member)> &filter) const {

    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    auto *cache = genericFilterCache<T>(map); // Returns nullptr if not class filtering

    size_t count = 0;
    if (not filter) { // Without lambda filtering the job is really easy
        count = cache
            ? cache->size() // Class filtering
            : map.size(); // No filtering at all
    }
    else if (cache) { // Class & lambda filtering
        for (auto &member : *cache) {
            if (filter(*dynamic_cast<T*>(member.get()))) count++;
        }
    }
    else {
        for (auto &m : map) {
            if (T* mem = dynamic_cast<T*>(m.second.get())) {
                // The cast succeeded, so this Member is also a T
                if (filter(*mem))
                    count++;
            }
        }
    }
    return count;
}

}


// vim:tw=100
