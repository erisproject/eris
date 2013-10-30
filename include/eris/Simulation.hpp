#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <typeinfo>
#include <list>

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
        Simulation();
        virtual ~Simulation() = default;
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
         *
         * \throws std::bad_cast if A cannot be cast to an Agent
         */
        template <class A, typename... Args> SharedMember<A> createAgent(Args&&... args);

        /** Makes a copy of the given A object, adds the copy to the simulation, and returns a
         * SharedMember<A> referencing it.  A must be a subclass of Agent.
         *
         * In practice, the template is typically omitted as it can be inferred from the argument.
         *
         * \throws std::bad_cast if A cannot be cast to an Agent
         */
        template <class A> SharedMember<A> cloneAgent(const A &a);

        /** Constructs a new G object using the given constructor arguments Args, adds it as a good,
         * and returns a SharedMember<G> referencing it.
         *
         * \throws std::bad_cast if G cannot be cast to a Good
         */
        template <class G, typename... Args> SharedMember<G> createGood(Args&&... args);

        /** Makes a copy of the given G object, adds the copy to the simulation, and returns a
         * SharedMember<G> referencing it.  G must be a subclass of Good.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         *
         * \throws std::bad_cast if G cannot be cast to a Good
         */
        template <class G> SharedMember<G> cloneGood(const G &g);

        /** Constructs a new M object using the given constructor arguments Args, adds it as a
         * market, and returns a SharedMember<M> referencing it.
         *
         * \throws std::bad_cast if M cannot be cast to a Market
         */
        template <class M, typename... Args> SharedMember<M> createMarket(Args&&... args);

        /** Makes a copy of the given M object, adds the copy to the simulation, and returns a
         * SharedMember<M> referencing it.  M must be a subclass of Market.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         *
         * \throws std::bad_cast if M cannot be cast to a Market
         */
        template <class M> SharedMember<M> cloneMarket(const M &m);

        /** Constructs a new intra-period optimizer object using the given constructor arguments
         * Args, adds it to the simulation, and returns a SharedMember<O> referencing it.
         */
        template <class O, typename... Args> SharedMember<O> createIntraOpt(Args&&... args);

        /** Makes a copy of the given O obejct, adds the copy to the simulation, and returns a
         * SharedMember<O> referencing it.  O must be a subclass of IntraOptimizer.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         *
         * \throws std::bad_cast if O cannot be cast to an IntraOptimizer.
         */
        template <class O> SharedMember<O> cloneIntraOpt(const O &o);

        /** Constructs a new inter-period optimizer object using the given constructor arguments
         * Args, adds it to the simulation, and returns a SharedMember<O> referencing it.
         */
        template <class O, typename... Args> SharedMember<O> createInterOpt(Args&&... args);

        /** Makes a copy of the given inter-period optimizer O obejct, adds the copy to the
         * simulation, and returns a SharedMember<O> referencing it.  O must be a subclass of
         * InterOptimizer.
         *
         * In practice, the template argument is omitted as it can be inferred from the argument.
         *
         * \throws std::bad_cast if O cannot be cast to an InterOptimizer.
         */
        template <class O> SharedMember<O> cloneInterOpt(const O &o);

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
         * simulation.  Note that both InterTemporal and SharedMember<InterTemporal> instances are
         * automatically cast to eris_id_t when required, so calling this method with those objects
         * as arguments is acceptable (and indeed preferred).
         */
        void removeInterOpt(const eris_id_t &oid);

        /** Provides read-only access to the map of all of the simulation's agents. */
        const MemberMap<Agent>& agents();
        /** Provides read-only access to the map of the simulation's goods. */
        const MemberMap<Good>& goods();
        /** Provides read-only access to the map of the simulation's markets. */
        const MemberMap<Market>& markets();
        /** Provides read-only access to the map of the simulation's intra-period optimization
         * objects. */
        const MemberMap<IntraOptimizer>& intraOpts();
        /** Provides read-only access to the map of the simulation's inter-period optimization
         * objects. */
        const MemberMap<InterOptimizer>& interOpts();

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

        /// Returns the map of dependencies.
        const DepMap& deps();

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
         * @param threads maximum threads to run.  Currently unimplemented.
         */
        void run(unsigned long max_threads = 1);

        /** Contains the number of rounds of the intra-period optimizers in the previous run() call.
         * A round is defined by a reset() call, a set of optimize() calls, and a set of
         * postOptimize() calls.  A multi-round optimization will only occur when there are
         * postOptimize()-implementing intraopt optimizers.
         *
         */
        int intraopt_count = -1;

    private:
        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        void insertIntraOpt(const SharedMember<IntraOptimizer> &opt);
        void insertInterOpt(const SharedMember<InterOptimizer> &opt);
        eris_id_t id_next_ = 1;
        std::unique_ptr<MemberMap<Agent>> agents_;
        std::unique_ptr<MemberMap<Good>> goods_;
        std::unique_ptr<MemberMap<Market>> markets_;
        std::unique_ptr<MemberMap<IntraOptimizer>> intraopts_;
        std::unique_ptr<MemberMap<InterOptimizer>> interopts_;

        template <class T, class B> const MemberMap<T>
        genericFilter(const MemberMap<B> &map, std::function<bool(SharedMember<T> member)> &filter) const;

        mutable std::unique_ptr<std::unordered_map<size_t, std::unordered_map<size_t, std::list<SharedMember<Member>>>>> filter_cache_;

        // Invalidate the filter cache for objects of type B, which should be one of the base Agent,
        // Good, Market, etc. member classes.  The default, Member, invalidates the cache for all
        // types.

        template <class B = Member,
                 class = typename std::enable_if<
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
};

}

// This has to be included here, because its templated methods require the Simulation class.
#include <eris/Member.hpp>

namespace eris {
template <class A, typename... Args> SharedMember<A> Simulation::createAgent(Args&&... args) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(std::forward<Args>(args)...));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class A> SharedMember<A> Simulation::cloneAgent(const A &a) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(a));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class G, typename... Args> SharedMember<G> Simulation::createGood(Args&&... args) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(std::forward<Args>(args)...));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class G> SharedMember<G> Simulation::cloneGood(const G &g) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(g));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class M, typename... Args> SharedMember<M> Simulation::createMarket(Args&&... args) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(std::forward<Args>(args)...));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class M> SharedMember<M> Simulation::cloneMarket(const M &m) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(m));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class O, typename... Args> SharedMember<O> Simulation::createIntraOpt(Args&&... args) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<IntraOptimizer> opt(new O(std::forward<Args>(args)...));
    insertIntraOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O> SharedMember<O> Simulation::cloneIntraOpt(const O &o) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<IntraOptimizer> opt(new O(o));
    insertIntraOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}

template <class O, typename... Args> SharedMember<O> Simulation::createInterOpt(Args&&... args) {
    // NB: Stored in a SM<InterOpt> rather than SM<O> ensures that O is an InterOptimizer subclass
    SharedMember<InterOptimizer> opt(new O(std::forward<Args>(args)...));
    insertInterOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O> SharedMember<O> Simulation::cloneInterOpt(const O &o) {
    // NB: Stored in a SM<InterOpt> rather than SM<O> ensures that O is an InterOptimizer subclass
    SharedMember<InterOptimizer> opt(new O(o));
    insertInterOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}

// Generic version of the various public ...Filter() methods that does the actual work.
template <class T, class B>
const Simulation::MemberMap<T> Simulation::genericFilter(
        const MemberMap<B>& map,
        std::function<bool(SharedMember<T> member)> &filter) const {
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
    return agents_->at(aid);
}

template <class G> SharedMember<G> Simulation::good(eris_id_t gid) const {
    return goods_->at(gid);
}

template <class M> SharedMember<M> Simulation::market(eris_id_t mid) const {
    return markets_->at(mid);
}

template <class I> SharedMember<I> Simulation::intraOpt(eris_id_t oid) const {
    return intraopts_->at(oid);
}

template <class I> SharedMember<I> Simulation::interOpt(eris_id_t oid) const {
    return interopts_->at(oid);
}
}

// vim:tw=100
