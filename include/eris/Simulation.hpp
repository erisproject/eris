#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <typeinfo>
#include <list>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace eris {

// Forward declarations
class Member;
class Agent;
class Good;
class Market;
class IntraOptimizer;
class InterOptimizer;

/** This class is at the centre of an Eris economy model; it keeps track of all of the agents
 * currently in the economy, all of the goods currently available in the economy, and the
 * interaction mechanisms (e.g. markets).  Note that all of these can change from one period to the
 * next.  It's also responsible for dispatching interactions (e.g. letting markets operate) and any
 * iteration-sensitive agent events (e.g. aging/dying/etc.).
 *
 * In short, this is the central piece of the Eris framework that dictates how all the other pieces
 * interact.
 */
class Simulation : public std::enable_shared_from_this<Simulation> {
    public:
        /// Creates a new Simulation.
        Simulation();
        // Destructor.  When destruction occurs, any outstanding threads are killed and rejoined.
        virtual ~Simulation();

        Simulation(const Simulation &) = delete;

        /// Alias for a map of eris_id_t to SharedMember<A> of arbitrary type A
        template <class T> using MemberMap = std::unordered_map<eris_id_t, SharedMember<T>>;
        /// typedef for the map of id's to the set of dependent members
        typedef std::unordered_map<eris_id_t, std::unordered_set<eris_id_t>> DepMap;

        /** Accesses an agent given the agent's eris_id_t.  Templated to allow conversion to
         * a SharedMember of the given Agent subclass; defaults to Agent.
         */
        template <class A = Agent> SharedMember<A> agent(eris_id_t aid) const;
        /** Accesses a good given the good's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Good subclass; defaults to Good.
         */
        template <class G = Good> SharedMember<G> good(eris_id_t gid) const;
        /** Accesses a market given the market's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Market subclass; defaults to Market.
         */
        template <class M = Market> SharedMember<M> market(eris_id_t mid) const;
        /** Accesses an intra-period optimization object given the object's eris_id_t.  Templated to
         * allow conversion to a SharedMember of the given IntraOptimizer subclass; defaults to
         * IntraOptimizer.
         */
        template <class I = IntraOptimizer> SharedMember<I> intraOpt(eris_id_t oid) const;
        /** Accesses an inter-period optimization object given the object's eris_id_t.  Templated to
         * allow conversion to a SharedMember of the given InterOptimizer subclass; defaults to
         * InterOptimizer.
         */
        template <class I = InterOptimizer> SharedMember<I> interOpt(eris_id_t oid) const;

        /** Constructs a new A object using the given constructor arguments Args, adds it as an
         * agent, and returns a SharedMember<A> referencing it.
         */
        template <class A, typename... Args, class = typename std::enable_if<std::is_base_of<Agent, A>::value>::type>
        SharedMember<A> createAgent(Args&&... args);

        /** Makes a copy of the given A object, adds the copy to the simulation, and returns a
         * SharedMember<A> referencing it.  A must be a subclass of Agent.
         *
         * In practice, the template is typically omitted as it can be inferred from the argument.
         */
        template <class A, class = typename std::enable_if<std::is_base_of<Agent, A>::value>::type>
        SharedMember<A> cloneAgent(const A &a);

        /** Constructs a new G object using the given constructor arguments Args, adds it as a good,
         * and returns a SharedMember<G> referencing it.
         */
        template <class G, typename... Args, class = typename std::enable_if<std::is_base_of<Good, G>::value>::type>
        SharedMember<G> createGood(Args&&... args);

        /** Makes a copy of the given G object, adds the copy to the simulation, and returns a
         * SharedMember<G> referencing it.  G must be a subclass of Good.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         */
        template <class G, class = typename std::enable_if<std::is_base_of<Good, G>::value>::type>
        SharedMember<G> cloneGood(const G &g);

        /** Constructs a new M object using the given constructor arguments Args, adds it as a
         * market, and returns a SharedMember<M> referencing it.
         */
        template <class M, typename... Args, class = typename std::enable_if<std::is_base_of<Market, M>::value>::type>
        SharedMember<M> createMarket(Args&&... args);

        /** Makes a copy of the given M object, adds the copy to the simulation, and returns a
         * SharedMember<M> referencing it.  M must be a subclass of Market.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         */
        template <class M, class = typename std::enable_if<std::is_base_of<Market, M>::value>::type>
        SharedMember<M> cloneMarket(const M &m);

        /** Constructs a new intra-period optimizer object using the given constructor arguments
         * Args, adds it to the simulation, and returns a SharedMember<O> referencing it.
         */
        template <class O, typename... Args, class = typename std::enable_if<std::is_base_of<IntraOptimizer, O>::value>::type>
        SharedMember<O> createIntraOpt(Args&&... args);

        /** Makes a copy of the given O obejct, adds the copy to the simulation, and returns a
         * SharedMember<O> referencing it.  O must be a subclass of IntraOptimizer.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         */
        template <class O, class = typename std::enable_if<std::is_base_of<IntraOptimizer, O>::value>::type>
        SharedMember<O> cloneIntraOpt(const O &o);

        /** Constructs a new inter-period optimizer object using the given constructor arguments
         * Args, adds it to the simulation, and returns a SharedMember<O> referencing it.
         */
        template <class O, typename... Args, class = typename std::enable_if<std::is_base_of<InterOptimizer, O>::value>::type>
        SharedMember<O> createInterOpt(Args&&... args);

        /** Makes a copy of the given inter-period optimizer O obejct, adds the copy to the
         * simulation, and returns a SharedMember<O> referencing it.  O must be a subclass of
         * InterOptimizer.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         */
        template <class O, class = typename std::enable_if<std::is_base_of<InterOptimizer, O>::value>::type>
        SharedMember<O> cloneInterOpt(const O &o);

        /** Removes the given agent (and any dependencies) from this simulation.  Note that both
         * Agent instances and SharedMember<Agent> instances are automatically cast to eris_id_t
         * when required, so calling this method with those objects is acceptable (and indeed
         * preferred).
         */
        void removeAgent(const eris_id_t &aid);
        /** Removes the given good (and any dependencies) from this simulation.  Note that both Good
         * instances and SharedMember<Good> instances are automatically cast to eris_id_t when
         * required, so calling this method with those objects is acceptable (and indeed preferred).
         */
        void removeGood(const eris_id_t &gid);
        /** Removes the given market (and any dependencies) from this simulation.  Note that both
         * Market instances and SharedMember<Market> instances are automatically cast to eris_id_t
         * when required, so calling this method with those objects is acceptable (and indeed
         * preferred).
         */
        void removeMarket(const eris_id_t &mid);
        /** Removes the given intra-temporal optimizer object (and any dependencies) from this
         * simulation.  Note that both IntraTemporal and SharedMember<IntraTemporal> instances are
         * automatically cast to eris_id_t when required, so calling this method with those objects
         * as arguments is acceptable (and indeed preferred).
         */
        void removeIntraOpt(const eris_id_t &oid);
        /** Removes the given inter-temporal optimizer object (and any dependencies) from this
         * simulation.  Note that both InterOptimizer and SharedMember<InterOptimizer> instances are
         * automatically cast to eris_id_t when required, so calling this method with those objects
         * as arguments is acceptable (and indeed preferred).
         */
        void removeInterOpt(const eris_id_t &oid);

        /** Provides a const map of eris_id_t to SharedMember<A>, filtered to only include
         * SharedMember<A> agents that induce a true return from the provided filter function.
         *
         * If the template parameter, A, is provided, it must be Agent or a subclass thereof: only
         * Agents that are instances of A will be considered and returned.
         *
         * If filter is null (the default if omitted), it will be treated as a filter that always
         * returns true; thus agentFilter<SomeAgentClass>() is a useful way to filter on just the
         * agent class type.
         *
         * The results of filtering on the class are cached so long as the template filter class is
         * not the default, `Agent`, so that subsequent calls don't need to search through all
         * agents for matching classes, but only check agents of the requested type.  This helps
         * considerably when searching for agents that are only a small subset of the overall set of
         * agents.  The cache is invalidated if any agent (of any type) is added or removed.
         */
        template <class A = Agent>
        const MemberMap<A> agentFilter(std::function<bool(SharedMember<A> agent)> filter = nullptr) const;
        /** Provides a filtered map of simulation goods.  This works just like agentFilter, but for
         * goods.
         *
         * \sa agentFilter
         */
        template <class G = Good>
        const MemberMap<G> goodFilter(std::function<bool(SharedMember<G> good)> filter = nullptr) const;
        /** Provides a filtered map of simulation markets.  This works just like agentFilter, but
         * for markets.
         *
         * \sa agentFilter
         */
        template <class M = Market>
        const MemberMap<M> marketFilter(std::function<bool(SharedMember<M> market)> filter = nullptr) const;
        /** Provides a filtered map of simulation intra-period optimizers.  This works just like
         * agentFilter, but for intra-period optimizers.
         *
         * \sa agentFilter
         */
        template <class I = IntraOptimizer>
        const MemberMap<I> intraOptFilter(std::function<bool(SharedMember<I> intraopt)> filter = nullptr) const;
        /** Provides a filtered map of simulation inter-period optimizers.  This works just like
         * agentFilter, but for inter-period optimizers.
         *
         * \sa agentFilter
         */
        template <class I = InterOptimizer>
        const MemberMap<I> interOptFilter(std::function<bool(SharedMember<I> interopt)> filter = nullptr) const;

        /** Records already-stored member `depends_on' as a dependency of `member'.  If `depends_on'
         * is removed from the simulation, `member' will be automatically removed as well.
         *
         * Note that dependents are removed *after* removal of their dependencies.  That is, if A
         * depends on B, and B is removed, the B removal occurs *first*, followed by the A removal.
         */
        void registerDependency(const eris_id_t &member, const eris_id_t &depends_on);

        /** Sets the maximum number of threads to use for subsequent calls to run().  The default
         * value is 0 (which uses no threads at all, see below).  If this is lowered between calls
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


        /** Thread allocation models.  There are currently three models available, which are
         * configured or queried by calling the threadModel() methods:
         *
         * - `ThreadModel::Preallocate` preallocates jobs in a fixed number across threads.  For
         *   example, if there are 100 intra-period optimizers and 4 threads, this will allocate 25
         *   jobs to each thread before beginning work.  The advantage of this method is that
         *   threads don't have to obtain a lock each time they access a new job, which
         *   substantially decreases thread contention, increasing threading performance when all
         *   jobs are tiny.
         * - `ThreadModel::Sequential` does no preallocation; each thread obtains a new task
         *   after finishing its current task from the global pool of waiting tasks.  When
         *   individual tasks take considerable but variable amounts of time, this is more efficient
         *   than preallocation as it tends to keep all threads busy for a shorter amount of time
         *   rather than having a few that happen to run longer.
         * - `ThreadModel::Hybrid` combines the above by looking at each job's associated
         *   preallocate*() method (e.g. InterOptimizer::preallocateApply(),
         *   IntraOptimizer::preallocateOptimize(), Agent::preallocateAdvance(), etc.),
         *   preallocating jobs (as ThreadModel::Preallocate does) that return true, queueing jobs
         *   (as in ThreadModel::Sequential) for jobs that return false.  This implicitly assumes
         *   that the preallocate*() methods are static: that is, that an object that returns true
         *   will always return true and vice versa, allowing the results to be cached.  If you
         *   have objects that sometimes return true and sometimes false, you might consider
         *   HybridRecheck instead.
         * - `ThreadModel::HybridRecheck` is just like Hybrid, except that it does not use queue
         *   caching: in each iteration the appropriate perallocate*() method must be checked to
         *   allocate jobs.  This induces a small performance hit, but may help with intertemporally
         *   heterogeneous jobs.
         */
        enum class ThreadModel { Preallocate, Sequential, Hybrid, HybridRecheck };

        /** Returns the currently active thread job ThreadModel.  The default model is currently
         * Hybrid.
         *
         * This is guaranteed not to change during a call to run().
         *
         * \sa ThreadModel
         */
        ThreadModel threadModel();

        /** Sets the thread job ThreadModel for the next run() call.  Has no effect if
         * threading is disabled (i.e. maxThreads() is 0).
         *
         * This cannot be changed during a call to run().
         *
         * \throws std::runtime_error if called during a run() call.
         */
        void threadModel(ThreadModel model);

        /** Returns the maximum number of threads that are can be used in the current run() call (if
         * called during run()) or in the next run() call (otherwise).  Returns 0 if threading is
         * disabled entirely.
         *
         * This is guaranteed not to change during a call to run().
         *
         * \sa maxThreads(unsigned long)
         */
        unsigned long maxThreads();

        /** Runs one period of period of the simulation.  The following happens, in order (except on
         * the first run, when the inter-period optimizer calls are skipped):
         *
         * - Inter-period optimization (except on first run):
         *   - All inter-period optimizers have their optimize() methods invoked.
         *   - All inter-period optimizers have apply() invoked.
         *   - All agents have their advance() method called.
         *   - All inter-period optimizers have postAdvance() called.
         * - Intra-period optimization:
         *   - All intra-period optimizers have initialize() called.
         *   - All intra-period optimizers have reset() called.
         *   - All intra-period optimizers have their optimize() methods called to calculate an
         *     optimal strategy (to be applied in apply()).
         *   - All intra-period optimizers have their postOptimize() methods called.
         *     - If one or more of the postOptimize() methods returns true, intra-period
         *       optimization restarts at the `reset()` stage..  Short-circuiting does *not* occur:
         *       all intra-period optimizers will run, even if some have already indicated a state
         *       change.
         *   - All intra-period optimizers have their apply() method called.
         *
         * \throws std::runtime_error if attempting to call run() during a run() call.
         */
        void run();

        /** Contains the number of rounds of the intra-period optimizers in the previous run() call.
         * A round is defined by a reset() call, a set of optimize() calls, and a set of
         * postOptimize() calls.  A multi-round optimization will only occur when there are
         * postOptimize()-implementing intraopt optimizers.
         *
         */
        int intraopt_count = -1;

    private:
        unsigned long max_threads_ = 0;
        ThreadModel thread_model_ = ThreadModel::Hybrid;
        bool running_ = false;
        eris_id_t id_next_ = 1;
        std::unique_ptr<MemberMap<Agent>> agents_;
        std::unique_ptr<MemberMap<Good>> goods_;
        std::unique_ptr<MemberMap<Market>> markets_;
        std::unique_ptr<MemberMap<IntraOptimizer>> intraopts_;
        std::unique_ptr<MemberMap<InterOptimizer>> interopts_;

        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        void insertIntraOpt(const SharedMember<IntraOptimizer> &opt);
        void insertInterOpt(const SharedMember<InterOptimizer> &opt);

        template <class T, class B> const MemberMap<T>
        genericFilter(const MemberMap<B> &map, std::function<bool(SharedMember<T> member)> &filter) const;

        mutable std::unique_ptr<std::unordered_map<size_t, std::unordered_map<size_t, std::list<SharedMember<Member>>>>> filter_cache_;

        // Invalidate the filter cache for objects of type B, which should be one of the base Agent,
        // Good, Market, etc. member classes.  The default, Member, invalidates the cache for all
        // types.

        template <class B = Member,
                 class = typename std::enable_if<
                     std::is_same<B, Member>::value or
                     std::is_same<B, Agent>::value or
                     std::is_same<B, Good>::value or
                     std::is_same<B, Market>::value or
                     std::is_same<B, IntraOptimizer>::value or
                     std::is_same<B, InterOptimizer>::value
                  >::type>
        void invalidateCache();

        DepMap depends_on_;
        void removeDeps(const eris_id_t &member);

        int iteration_ = 0;

        /* Threading variables */

        // Protects member access/updates
        mutable std::recursive_mutex member_mutex_;

        // Pool of threads we can use
        std::vector<std::thread> thr_pool_;

        // The different stages of the simulation, for synchronizing threads
        enum class RunStage {
            idle, // between-period/initial thread state
            kill, // When a thread sees this, it checks thr_kill_, and if it is the current thread id, it finishes.
            kill_all, // When a thread sees this, it finishes.
            // Inter-period optimization stages (inter_advance applies to agents):
            inter_optimize, inter_apply, inter_advance, inter_postAdvance,
            // Intra-period optimization stages:
            intra_initialize, intra_reset, intra_optimize, intra_postOptimize, intra_apply
        };

        // The current experiment stage
        RunStage stage_ = RunStage::idle;

        // The thread to quit when a kill stage is signalled
        std::thread::id thr_kill_;

        // Mutex controlling access to thread variables, members, and synchronization
        std::mutex thr_sync_mutex_;

        // Queue of the eris_id_ts of member object (Agent, InterOpt, or IntraOpt) still waiting to
        // be picked up by a thread for processing in the current stage.  The queues are per-thread,
        // and are used only in preallocate and hybrid threading models.
        //
        // This object is updated only be updated by the master thread, and only while all children
        // are dormant.
        //
        // The vector is actually a shared pointer to allow caching of prior values as a speed-up.
        std::unordered_map<std::thread::id, std::shared_ptr<std::vector<eris_id_t>>> thr_q_;

        // Shared queue for sequential and hybrid threading models
        std::shared_ptr<std::vector<eris_id_t>> shared_q_;
        // Position of the next element to be processed in the shared_q_
        std::atomic_size_t shared_q_next_;

        // Cache of agent, interopt, and intraopt ids for the shared q cache.  This cache is used
        // when it exists; otherwise the list is rebuilt from the simulation's current set of
        // agents/interopts/intraopts.  The cache is invalidated when an agent/interopt/intraopt is
        // added or removed.
        std::shared_ptr<std::vector<eris_id_t>> shared_q_cache_agent_, shared_q_cache_inter_, shared_q_cache_intra_;

        // Set to true when the relevant cache needs to be recalculated
        bool q_cache_reset_agent_ = true, q_cache_reset_inter_ = true, q_cache_reset_intra_ = true;

        // Same as the above, but for the per-thread caches
        std::unordered_map<std::thread::id, std::shared_ptr<std::vector<eris_id_t>>> thr_cache_agent_, thr_cache_inter_, thr_cache_intra_;

        // The number of threads finished the current stage; when this reaches the size of
        // thr_pool_, the stage is finished.
        unsigned int thr_done_ = 0;

        // Will be set the false at the beginning of an intraopt round.  If a postOptimize returns
        // true (to restart the round), this will end up as true again; if still false at the end of
        // the postOptimize stage, the intraopt stage ends, otherwise it restarts.
        bool thr_redo_intra_ = true;

        // CV for signalling a change in stage from master to workers
        std::condition_variable_any thr_cv_stage_;

        // CV for signalling the master that a thread has finished
        std::condition_variable_any thr_cv_done_;

        // Sets up the thread and/or shared queues for the appropriate stage.  This starts up
        // threads if needed (up to `max` or the number of members, whichever is smaller), sets
        // stage_, signals the threads to start, then wait for all threads to have signalled that
        // they are finished with the current stage.  thr_sync_mutex_ should be locked when calling,
        // and will be locked when this returns.
        void thr_stage(const RunStage &stage);

        // Loads the given eris_id_t into the given thread cache at position next, incrementing next
        // (and resetting to zero when it hits the end of the number of threads).
        void thr_queue(size_t &next, decltype(thr_cache_agent_) &thr_cache, const eris_id_t &id) const;

        // Called at the beginning of run() to start up needed threads or kill off excess threads.
        //
        // Excess threads are those which exceed maxThreads().
        //
        // Needed threads is the smaller of maxThreads() and maximum simultaneous jobs, which is the
        // maximum of the number of agents, interoptimizers, and intraoptimizers.
        //
        // Note that those numbers two values don't necessarily coincide.
        //
        // If this method changes the number of threads, this also takes care to invalidate queue
        // caches as needed so that appropriate thread reallocations will occur.
        //
        // This is called at the beginning of run().
        void thr_thread_pool();

        // Runs a stage without using threads.  This is called instead of thr_stage() when
        // maxThreads() is set to 0.
        void nothr_stage(const RunStage &stage);

        // The main thread loop; runs until it sees a RunStage::kill with thr_kill_ set to the
        // thread's id.
        void thr_loop();

        // Checks the thread-specific queue and, if non-empty, releases the sync lock until it
        // finishes the agents from it.  Once finished, it tries to pull agents from the master
        // thread queue (if any), releasing the sync lock for one job at a time, until the shared
        // queue is also empty.
        // Pulls the next Agent from the Agent queue, calls work on it, repeats until the queue is
        // empty.  The sync mutex must be locked before calling and will be unlocked for the
        // duration of each work call, then locked again.
        void thr_work_agent(const std::function<void(Agent&)> &work);
        // Just like above, but for the InterOpt queue
        void thr_work_inter(const std::function<void(InterOptimizer&)> &work);
        // Just like above, but for the IntraOpt queue
        void thr_work_intra(const std::function<void(IntraOptimizer&)> &work);

        // Used to signal that the stage has finished.  After signalling, this calls thr_wait() to
        // wait until the stage changes to anything other than its current value.
        void thr_stage_finished();

        // Waits until stage changes to something other than the stage at the time of the call.
        void thr_wait();
};

}

// This has to be included here, because its templated methods require the Simulation class.
#include <eris/Member.hpp>

namespace eris {
template <class A, typename... Args, class> SharedMember<A> Simulation::createAgent(Args&&... args) {
    SharedMember<A> agent(std::make_shared<A>(std::forward<Args>(args)...));
    insertAgent(agent);
    return agent;
}

template <class A, class> SharedMember<A> Simulation::cloneAgent(const A &a) {
    SharedMember<A> agent(std::make_shared<A>(a));
    insertAgent(agent);
    return agent;
}

template <class G, typename... Args, class> SharedMember<G> Simulation::createGood(Args&&... args) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(std::make_shared<G>(std::forward<Args>(args)...));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class G, class> SharedMember<G> Simulation::cloneGood(const G &g) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(std::make_shared<G>(g));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class M, typename... Args, class> SharedMember<M> Simulation::createMarket(Args&&... args) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(std::make_shared<M>(std::forward<Args>(args)...));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class M, class> SharedMember<M> Simulation::cloneMarket(const M &m) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(std::make_shared<M>(m));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class O, typename... Args, class> SharedMember<O> Simulation::createIntraOpt(Args&&... args) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<IntraOptimizer> opt(std::make_shared<O>(std::forward<Args>(args)...));
    insertIntraOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O, class> SharedMember<O> Simulation::cloneIntraOpt(const O &o) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<O> opt(std::make_shared<O>(o));
    insertIntraOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}

template <class O, typename... Args, class> SharedMember<O> Simulation::createInterOpt(Args&&... args) {
    SharedMember<O> opt(std::make_shared<O>(std::forward<Args>(args)...));
    insertInterOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O, class> SharedMember<O> Simulation::cloneInterOpt(const O &o) {
    // NB: Stored in a SM<InterOpt> rather than SM<O> ensures that O is an InterOptimizer subclass
    SharedMember<InterOptimizer> opt(std::make_shared<O>(o));
    insertInterOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}

// Generic version of the various public ...Filter() methods that does the actual work.
template <class T, class B>
const Simulation::MemberMap<T> Simulation::genericFilter(
        const MemberMap<B>& map,
        std::function<bool(SharedMember<T> member)> &filter) const {

    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    MemberMap<T> matched;
    auto &B_t = typeid(B), &T_t = typeid(T);
    size_t B_h = B_t.hash_code(), T_h = T_t.hash_code();
    bool class_filtering = B_t != T_t;
    std::list<SharedMember<Member>> cache;
    bool cache_miss = false;
    if (class_filtering) {
        // Initiliaze the cache, if necessary
        if (!filter_cache_)
            filter_cache_ = std::unique_ptr<
                      std::unordered_map<size_t, std::unordered_map<size_t, std::list<SharedMember<Member>>>>
                >(new std::unordered_map<size_t, std::unordered_map<size_t, std::list<SharedMember<Member>>>>());

        // Initialize the B-specific part of the cache, if necessary:
        if (filter_cache_->count(B_h) == 0)
            filter_cache_->insert(std::pair<size_t, std::unordered_map<size_t, std::list<SharedMember<Member>>>>(
                        B_h, std::unordered_map<size_t, std::list<SharedMember<Member>>>()));

        auto &filter_cache_B_ = filter_cache_->at(B_h);

        // Look to see if there's a cache for T already; if there isn't, we'll build one in `cache`
        if (filter_cache_B_.count(T_h) == 0) {
            cache_miss = true;
        }
    }

    if (class_filtering and not cache_miss) {
        for (auto &member : filter_cache_->at(B_h).at(T_h)) {
            SharedMember<T> recast(member);
            if (not filter or filter(recast))
                matched.insert(std::make_pair(recast->id(), recast));
        }
    }
    else {
        for (auto &m : map) {
            bool cast_success = false;
            try {
                SharedMember<T> recast(m.second);
                cast_success = true;
                if (cache_miss)
                    cache.push_back(recast);

                if (not filter or filter(recast))
                    matched.insert(std::make_pair(recast->id(), recast));
            }
            catch (std::bad_cast &e) {
                if (cast_success) {
                    // The bad_cast is *not* from the recast, so rethrow it
                    throw;
                }
                // Otherwise the agent isn't castable to T, so just move on.
            }
        }

        if (cache_miss) filter_cache_->at(B_h).insert(std::make_pair(T_h, cache));
    }
    return matched;
}

template <class B, class>
void Simulation::invalidateCache() {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    if (filter_cache_) {
        if (typeid(B) == typeid(Member))
            filter_cache_->clear();
        else
            filter_cache_->erase(typeid(B).hash_code());
    }
}

template <class A>
const Simulation::MemberMap<A> Simulation::agentFilter(std::function<bool(SharedMember<A> agent)> filter) const {
    return genericFilter(*agents_, filter);
}
template <class G>
const Simulation::MemberMap<G> Simulation::goodFilter(std::function<bool(SharedMember<G> good)> filter) const {
    return genericFilter(*goods_, filter);
}
template <class M>
const Simulation::MemberMap<M> Simulation::marketFilter(std::function<bool(SharedMember<M> market)> filter) const {
    return genericFilter(*markets_, filter);
}
template <class I>
const Simulation::MemberMap<I> Simulation::interOptFilter(std::function<bool(SharedMember<I> interopt)> filter) const {
    return genericFilter(*interopts_, filter);
}
template <class I>
const Simulation::MemberMap<I> Simulation::intraOptFilter(std::function<bool(SharedMember<I> intraopt)> filter) const {
    return genericFilter(*intraopts_, filter);
}

template <class A> SharedMember<A> Simulation::agent(eris_id_t aid) const {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    return SharedMember<A>(agents_->at(aid));
}

template <class G> SharedMember<G> Simulation::good(eris_id_t gid) const {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    return SharedMember<G>(goods_->at(gid));
}

template <class M> SharedMember<M> Simulation::market(eris_id_t mid) const {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    return SharedMember<M>(markets_->at(mid));
}

template <class I> SharedMember<I> Simulation::intraOpt(eris_id_t oid) const {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    return SharedMember<I>(intraopts_->at(oid));
}

template <class I> SharedMember<I> Simulation::interOpt(eris_id_t oid) const {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    return SharedMember<I>(interopts_->at(oid));
}

inline unsigned long Simulation::maxThreads() { return max_threads_; }



inline void Simulation::thr_stage_finished() {
    if (maxThreads() > 0) {
        thr_done_++;
        thr_cv_done_.notify_all();
        thr_wait();
    }
}

inline void Simulation::thr_wait() {
    RunStage curr_stage = stage_;
    thr_cv_stage_.wait(thr_sync_mutex_, [this,curr_stage] { return stage_ != curr_stage; });
}

inline void Simulation::thr_queue(size_t &next, decltype(thr_cache_agent_) &thr_cache, const eris_id_t &id) const {
    thr_cache[thr_pool_.at(next).get_id()]->push_back(id);
    next = (++next) % thr_pool_.size();
}

}


// vim:tw=100
