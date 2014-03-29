#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <eris/debug.hpp>
#include <eris/noncopyable.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace eris {

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
        /// Destructor.  When destruction occurs, any outstanding threads are killed and rejoined.
        virtual ~Simulation();

        /// Alias for a map of eris_id_t to SharedMember<A> of arbitrary type A
        template <class T> using MemberMap = std::unordered_map<eris_id_t, SharedMember<T>>;
        /// typedef for the map of id's to the set of dependent members
        typedef std::unordered_map<eris_id_t, std::unordered_set<eris_id_t>> DepMap;

        /** Accesses an agent given the agent's eris_id_t.  Templated to allow conversion to
         * a SharedMember of the given Agent subclass; defaults to Agent.
         */
        template <class A = Agent, typename = typename std::enable_if<std::is_base_of<Agent, A>::value>::type>
        SharedMember<A> agent(const eris_id_t &aid) const;
        /** Accesses a good given the good's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Good subclass; defaults to Good.
         */
        template <class G = Good, typename = typename std::enable_if<std::is_base_of<Good, G>::value>::type>
        SharedMember<G> good(const eris_id_t &gid) const;
        /** Accesses a market given the market's eris_id_t.  Templated to allow conversion to a
         * SharedMember of the given Market subclass; defaults to Market.
         */
        template <class M = Market, typename = typename std::enable_if<std::is_base_of<Market, M>::value>::type>
        SharedMember<M> market(const eris_id_t &mid) const;
        /** Accesses a non-agent/good/market member that has been added to this simulation.  This is
         * typically an optimization object.
         */
        template <class O = Member, typename = typename std::enable_if<
            std::is_base_of<Member, O>::value and not std::is_base_of<Agent, O>::value and not std::is_base_of<Market, O>::value
            >::type>
        SharedMember<O> other(const eris_id_t &oid) const;

        /** Constructs a new T object using the given constructor arguments Args, adds it to the
         * simulation.  T must be a subclass of Member; if it is also a subclass of Agent, Good, or
         * Market it will be treated as the appropriate type; otherwise it is treated as an "other"
         * member.
         *
         * Example:
         *     auto money = sim->create<Good::Continuous>("money");
         *     auto good = sim->create<Good::Continuous>("x");
         *     auto market = sim->create<Bertrand>(Bundle(good, 1), Bundle(money, 1));
         */
        template <class T, typename... Args, class = typename std::enable_if<std::is_base_of<Member, T>::value>::type>
        SharedMember<T> create(Args&&... args);

        /** Removes the given member (and any dependencies) from this simulation.
         *
         * \throws std::out_of_range if the given id does not belong to this simulation.
         */
        void remove(const eris_id_t &id);

        /** Provides a vector of SharedMember<A>, optionally filtered to only include agents that
         * induce a true return from the provided filter function.
         *
         * The template class, A, provides a second sort of filtering: if it is provided (it must be
         * a class ultimately derived from Agent), only agents of type A will be returned.  If the
         * filter is also provided, only A objects will be passed to the filter and only returned if
         * the filter returns true.
         *
         * The results of class filtering are cached so long as the template filter class is not the
         * default (`Agent`) so that subsequent calls don't need to search through all agents for
         * matching classes, but only the agents of the appropriate type.  This helps considerably
         * when searching for agents whose type is only a small subset of the overall set of agents.
         * The cache is invalidated when any agent (of any type) is added or removed.
         */
        template <class A = Agent, typename = typename std::enable_if<std::is_base_of<Agent, A>::value>::type>
        std::vector<SharedMember<A>> agents(const std::function<bool(const A &agent)> &filter = nullptr) const;
        /** Provides a count of matching simulation agents.  Agents are filtered by class and/or
         * callable filter and the count of matching agents is returned.  This is equivalent to
         * agents<A>(filter).size(), but more efficient.
         */
        template <class A = Agent, typename = typename std::enable_if<std::is_base_of<Agent, A>::value>::type>
        size_t countAgents(const std::function<bool(const A &agent)> &filter = nullptr) const;

        /** Provides a filtered vector of simulation goods.  This works just like agents(), but for
         * goods.
         *
         * \sa agents()
         */
        template <class G = Good, typename = typename std::enable_if<std::is_base_of<Good, G>::value>::type>
        std::vector<SharedMember<G>> goods(const std::function<bool(const G &good)> &filter = nullptr) const;
        /** Provides a count of matching simulation goods.  Goods are filtered by class and/or
         * callable filter and the count of matching goods is returned.  This is equivalent to
         * goods<G>(filter).size(), but more efficient.
         */
        template <class G = Good, typename = typename std::enable_if<std::is_base_of<Good, G>::value>::type>
        size_t countGoods(const std::function<bool(const G &good)> &filter = nullptr) const;

        /** Provides a filtered vector of simulation markets.  This works just like agents(), but
         * for markets.
         *
         * \sa agents()
         */
        template <class M = Market, typename = typename std::enable_if<std::is_base_of<Market, M>::value>::type>
        std::vector<SharedMember<M>> markets(const std::function<bool(const M &market)> &filter = nullptr) const;
        /** Provides a count of matching simulation markets.  Markets are filtered by class and/or
         * callable filter and the count of matching markets is returned.  This is equivalent to
         * markets<G>(filter).size(), but more efficient.
         */
        template <class M = Market, typename = typename std::enable_if<std::is_base_of<Market, M>::value>::type>
        size_t countMarkets(const std::function<bool(const M &good)> &filter = nullptr) const;

        /** Provides a filtered vector of non-agent/good/market simulation objects.  This works just like
         * agentFilter, but for non-agent/good/market members.
         *
         * \sa agentFilter
         */
        template <class O = Member, typename = typename std::enable_if<
            std::is_base_of<Member, O>::value and not std::is_base_of<Agent, O>::value and not std::is_base_of<Market, O>::value
            >::type>
        std::vector<SharedMember<O>> others(const std::function<bool(const O &other)> &filter = nullptr) const;
        /** Provides a count of matching simulation non-agent/good/market members.  Members are
         * filtered by class and/or callable filter and the count of matching members is returned.
         * This is equivalent to members<G>(filter).size(), but more efficient.
         */
        template <class O = Member, typename = typename std::enable_if<
            std::is_base_of<Member, O>::value and not std::is_base_of<Agent, O>::value and not std::is_base_of<Market, O>::value
            >::type>
        size_t countOthers(const std::function<bool(const O &good)> &filter = nullptr) const;

        /** Records already-stored member `depends_on' as a dependency of `member'.  If `depends_on'
         * is removed from the simulation, `member' will be automatically removed as well.
         *
         * Note that dependents are removed *after* removal of their dependencies.  That is, if A
         * depends on B, and B is removed, the B removal occurs *first*, followed by the A removal.
         */
        void registerDependency(const eris_id_t &member, const eris_id_t &depends_on);

        /** Records already-stored member `depends_on' is a weak dependency of `member'.  In
         * contrast to a non-weak dependency, the member is only notified of the removal of the
         * dependent, it is not removed itself.
         *
         * When `depends_on' is removed from the simulation, `member' will have its depRemoved()
         * method called with the just-removed (but still not destroyed) member.
         */
        void registerWeakDependency(const eris_id_t &member, const eris_id_t &depends_on);

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

        /** Returns the iteration number, where 1 is the first iteration.  This is incremented
         * before inter-period optimizers run, and so, if called from inter-period optimizer code,
         * this will return the t value for the upcoming stage, not the most recent stage.
         *
         * This is initially 0 (until the first run() call).
         */
        unsigned long t() const;

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

        /** Obtains a lock that, when held, guarantees that a simulation stage is not in progress.
         * This is designed for external code (e.g. GUI displays) to sychronize code, ensuring that
         * it does not run simultaneously with an active run call.  This lock is held internally
         * during run().
         */
        std::unique_lock<std::mutex> runLock();

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

    protected:
        /** Constructs a new simulation.  This is protected because it must be called via
         * Eris<Simulation> wrapper.  (This is needed because there must be a shared_ptr active on
         * the simulation at all times).
         */
        Simulation() = default;
        friend class Eris<Simulation>;

    private:
        unsigned long max_threads_ = 0;
        bool running_ = false;
        eris_id_t id_next_ = 1;
        MemberMap<Agent> agents_;
        MemberMap<Good> goods_;
        MemberMap<Market> markets_;
        MemberMap<Member> others_;

        // insert() decides which of following insertAgent, insertGood, etc. methods to call and
        // calls it.  Called from the public create() methods.
        void insert(const SharedMember<Member> &member);

        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        void insertOther(const SharedMember<Member> &other);

        // Internal method to remove one of the various types.  Called by the public remove()
        // method, which figures out which type the removal request is for.
        void removeAgent(const eris_id_t &aid);
        void removeGood(const eris_id_t &gid);
        void removeMarket(const eris_id_t &mid);
        void removeOther(const eris_id_t &oid);

        // Determines which (if any) optimization interfaces the member implements, and records it
        // for the next optimization stage.
        void insertOptimizers(const SharedMember<Member> &member);

        // Undoes insertOptimizers.
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

        template <class B,
                 class = typename std::enable_if<
                     std::is_same<B, Member>::value or
                     std::is_same<B, Agent>::value or
                     std::is_same<B, Good>::value or
                     std::is_same<B, Market>::value
                  >::type>
        void invalidateCache();

        DepMap depends_on_, weak_dep_;

        // Removes hard dependents
        void removeDeps(const eris_id_t &member);

        // Notifies weak dependents
        void notifyWeakDeps(SharedMember<Member> member, const eris_id_t &old_id);

        // Tracks the iteration number, can be accessed via t().
        unsigned long iteration_ = 0;

        /* Threading variables */

        // Protects member access/updates
        mutable std::recursive_mutex member_mutex_;

        // Mutex held during a run which is available for outside threads to ensure operation not
        // during an active stage.  See runLock().
        std::mutex run_mutex_;

        // Pool of threads we can use
        std::vector<std::thread> thr_pool_;

        // The current experiment stage
        std::atomic<RunStage> stage_{RunStage::idle};

        // The Members implementing each optimization stage; indexes are RunStage values
        std::vector<std::set<SharedMember<Member>>> optimizers_{1 + (int) RunStage_LAST};

        // The thread to quit when a kill stage is signalled
        std::atomic<std::thread::id> thr_kill_;

        // Caches of the various optimizer types; these get wiped out when any optimizers of the
        // given type are added or removed.  Indexes are RunStage values.
        std::vector<std::shared_ptr<std::vector<SharedMember<Member>>>> shared_q_cache_{1 + (int) RunStage_LAST};
        // Shared queue for the current stage; this will be a copied shared_ptr from shared_q_cache_
        std::shared_ptr<std::vector<SharedMember<Member>>> shared_q_;
        // Position of the next element to be processed in *shared_q_
        std::atomic_size_t shared_q_next_;

        // The number of threads finished the current stage; when this reaches the size of
        // thr_pool_, the stage is finished.
        unsigned int thr_running_ = 0;

        // Will be set the false at the beginning of an intraopt round.  If a postOptimize returns
        // true (to restart the round), this will end up as true again; if still false at the end of
        // the postOptimize stage, the intraopt stage ends, otherwise it restarts.
        std::atomic_bool thr_redo_intra_{true};

        // CV for signalling a change in stage from master to workers
        std::condition_variable thr_cv_stage_;
        // mutex for the stage CV and for threads waiting for changes in stage_
        std::mutex stage_mutex_;

        // CV for signalling the master that a thread has finished
        std::condition_variable thr_cv_done_;
        // mutex for the done CV and for threads signalling that they have finished
        std::mutex done_mutex_;

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
        // This is called at the beginning of run().
        void thr_thread_pool();

        // Runs a stage without using threads.  This is called instead of thr_stage() when
        // maxThreads() is set to 0.
        void nothr_stage(const RunStage &stage);

        // The main thread loop; runs until it sees a RunStage::kill with thr_kill_ set to the
        // thread's id.
        void thr_loop();

        // Called by a thread to process the current shared_q_ of waiting optimization objects.
        template <class Opt>
        void thr_work(const std::function<void(Opt&)> &work);

        // Used to signal that the stage has finished.  After signalling, this calls
        // thr_wait(curr_stage) to wait until the stage changes to something other than curr_stage
        void thr_stage_finished(const RunStage &curr_stage);

        // Waits until stage changes to something other than the passed-in stage.
        void thr_wait(const RunStage &curr_stage);
};


template <class T, typename... Args, class> SharedMember<T> Simulation::create(Args&&... args) {
    SharedMember<T> member(std::make_shared<T>(std::forward<Args>(args)...));
    insert(member);
    return member;
}

// These methods are all basically identical for the four core types (agent, good, market, other),
// so use a macro.  TYPE (agent, good, etc.) is used to create a TYPE and a TYPEs method (e.g.
// agent() and agents()).
//
// Searching help:
// agent() agents() good() goods() market() markets() other() others()
#define ERIS_SIM_TYPE_METHODS(TYPE, CAP_TYPE)\
template <class T, typename> SharedMember<T> Simulation::TYPE(const eris_id_t &id) const {\
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);\
    return SharedMember<T>(TYPE##s_.at(id));\
}\
template <class T, typename> std::vector<SharedMember<T>> Simulation::TYPE##s(const std::function<bool(const T &TYPE)> &filter) const {\
    return genericFilter(TYPE##s_, filter);\
}\
template <class T, typename> size_t Simulation::count##CAP_TYPE##s(const std::function<bool(const T &TYPE)> &filter) const {\
    return genericFilterCount(TYPE##s_, filter);\
}
ERIS_SIM_TYPE_METHODS(agent, Agent)
ERIS_SIM_TYPE_METHODS(good, Good)
ERIS_SIM_TYPE_METHODS(market, Market)
ERIS_SIM_TYPE_METHODS(other, Other)
#undef ERIS_SIM_TYPE_METHODS

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
            if (dynamic_cast<T*>(m.second.ptr_.get())) {
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
            if (T* mem = dynamic_cast<T*>(m.second.ptr_.get())) {
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
            if (filter(*dynamic_cast<T*>(member.ptr_.get()))) count++;
        }
    }
    else {
        for (auto &m : map) {
            if (T* mem = dynamic_cast<T*>(m.second.ptr_.get())) {
                // The cast succeeded, so this Member is also a T
                if (filter(*mem))
                    count++;
            }
        }
    }
    return count;
}

template <class B, class>
void Simulation::invalidateCache() {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    filter_cache_.erase(std::type_index(typeid(B)));
}

inline unsigned long Simulation::maxThreads() { return max_threads_; }



inline void Simulation::thr_stage_finished(const RunStage &curr_stage) {
    if (maxThreads() > 0) {
        {
            std::unique_lock<std::mutex> lock(done_mutex_);
            if (--thr_running_ == 0)
                thr_cv_done_.notify_one();
        }
        thr_wait(curr_stage);
    }
}

inline void Simulation::thr_wait(const RunStage &curr_stage) {
    std::unique_lock<std::mutex> lock(stage_mutex_);
    if (stage_ == curr_stage)
        thr_cv_stage_.wait(lock, [this,curr_stage] { return stage_ != curr_stage; });
}

inline unsigned long Simulation::t() const {
    return iteration_;
}

}


// vim:tw=100
