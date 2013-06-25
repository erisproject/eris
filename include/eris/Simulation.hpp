#pragma once
#include <eris/types.hpp>
#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/Optimizer.hpp>
#include <unordered_map>
#include <unordered_set>
#include <memory>

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
class Simulation : public std::enable_shared_from_this<Simulation> {
    public:
        //Simulation();
        virtual ~Simulation() = default;

        /// typedef for the map of id's to shared goods
        typedef std::unordered_map<eris_id_t, SharedMember<Good>> GoodMap;
        /// typedef for the map of id's to shared agents
        typedef std::unordered_map<eris_id_t, SharedMember<Agent>> AgentMap;
        /// typedef for the map of id's to shared markets
        typedef std::unordered_map<eris_id_t, SharedMember<Market>> MarketMap;
        /// typedef for the map of id's to shared intraperiod optimizations
        typedef std::unordered_map<eris_id_t, SharedMember<IntraOptimizer>> IntraOptMap;
        /// typedef for the map of id's to shared interperiod optimizations
        typedef std::unordered_map<eris_id_t, SharedMember<InterOptimizer>> InterOptMap;
        /// typedef for the map of id's to the set of dependent members
        typedef std::unordered_map<eris_id_t, std::unordered_set<eris_id_t>> DepMap;

        /// Accesses an agent given the agent's eris_id_t
        SharedMember<Agent> agent(eris_id_t aid);
        /// Accesses a good given the good's eris_id_t
        SharedMember<Good> good(eris_id_t gid);
        /// Accesses a market given the market's eris_id_t
        SharedMember<Market> market(eris_id_t mid);
        /// Accesses an intra-period optimization object given the object's eris_id_t
        SharedMember<IntraOptimizer> intraOpt(eris_id_t oid);
        /// Accesses an inter-period optimization object given the object's eris_id_t
        SharedMember<InterOptimizer> interOpt(eris_id_t oid);

        /** Constructs a new A object using the given constructor arguments Args, adds it as an
         * agent, and returns a SharedMember<A> referencing it.
         *
         * \throws std::bad_cast if A cannot be cast to an Agent
         */
        template <class A, typename... Args> SharedMember<A> createAgent(const Args&... args);

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
        template <class G, typename... Args> SharedMember<G> createGood(const Args&... args);

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
        template <class M, typename... Args> SharedMember<M> createMarket(const Args&... args);

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
        template <class O, typename... Args> SharedMember<O> createIntraOpt(const Args&... args);

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
        template <class O, typename... Args> SharedMember<O> createInterOpt(const Args&... args);

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

        /** Provides read-only access to the map of the simulation's agents. */
        const AgentMap& agents();
        /** Provides read-only access to the map of the simulation's goods. */
        const GoodMap& goods();
        /** Provides read-only access to the map of the simulation's markets. */
        const MarketMap& markets();
        /** Provides read-only access to the map of the simulation's intra-period optimization
         * objects. */
        const IntraOptMap& intraOpts();
        /** Provides read-only access to the map of the simulation's inter-period optimization
         * objects. */
        const InterOptMap& interOpts();

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
         * the first run, when the first 3 are skipped):
         *
         * - All inter-period optimizers have their optimize() methods invoked.
         * - All agents have their advance() method called.
         * - All inter-period optimizers have apply() invoked.
         * - All intra-period optimizers have reset() called.
         * - All intra-period optimizers have their optimize() methods called until all optimizers
         *   return false.  If *any* optimizer returns true, all optimizers will be called again.
         *   The specific pattern and order of these calls is not guaranteed.
         */
        void run();

        /// Accesses the number of run throughs of the intra-period optimizers in the previous run() call.
        int intraopt_loops = -1;

    private:
        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        void insertIntraOpt(const SharedMember<IntraOptimizer> &opt);
        void insertInterOpt(const SharedMember<InterOptimizer> &opt);
        eris_id_t id_next_ = 1;
        AgentMap agents_;
        GoodMap goods_;
        MarketMap markets_;
        IntraOptMap intraopts_;
        InterOptMap interopts_;

        DepMap depends_on_;
        void removeDeps(const eris_id_t &member);

        int iteration_ = 0;
};

template <class A, typename... Args> SharedMember<A> Simulation::createAgent(const Args&... args) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(args...));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class A> SharedMember<A> Simulation::cloneAgent(const A &a) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(a));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class G, typename... Args> SharedMember<G> Simulation::createGood(const Args&... args) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(args...));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class G> SharedMember<G> Simulation::cloneGood(const G &g) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(g));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class M, typename... Args> SharedMember<M> Simulation::createMarket(const Args&... args) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(args...));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class M> SharedMember<M> Simulation::cloneMarket(const M &m) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(m));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class O, typename... Args> SharedMember<O> Simulation::createIntraOpt(const Args&... args) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<IntraOptimizer> opt(new O(args...));
    insertIntraOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O> SharedMember<O> Simulation::cloneIntraOpt(const O &o) {
    // NB: Stored in a SM<IntraOpt> rather than SM<O> ensures that O is an IntraOptimizer subclass
    SharedMember<IntraOptimizer> opt(new O(o));
    insertIntraOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}

template <class O, typename... Args> SharedMember<O> Simulation::createInterOpt(const Args&... args) {
    // NB: Stored in a SM<InterOpt> rather than SM<O> ensures that O is an InterOptimizer subclass
    SharedMember<InterOptimizer> opt(new O(args...));
    insertInterOpt(opt);
    return opt; // Implicit recast back to SharedMember<O>
}

template <class O> SharedMember<O> Simulation::cloneInterOpt(const O &o) {
    // NB: Stored in a SM<InterOpt> rather than SM<O> ensures that O is an InterOptimizer subclass
    SharedMember<InterOptimizer> opt(new O(o));
    insertInterOpt(opt);
    return market; // Implicit recast back to SharedMember<O>
}


}
