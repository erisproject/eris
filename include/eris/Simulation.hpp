#pragma once
#include <eris/types.hpp>
#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <exception>
#include <map>
#include <memory>
#include <type_traits>

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

        typedef std::map<eris_id_t, SharedMember<Good>> GoodMap;
        typedef std::map<eris_id_t, SharedMember<Agent>> AgentMap;
        typedef std::map<eris_id_t, SharedMember<Market>> MarketMap;

        /// Accesses an agent given the agent's eris_id_t
        SharedMember<Agent> agent(eris_id_t aid);
        /// Accesses a good given the good's eris_id_t
        SharedMember<Good> good(eris_id_t gid);
        /// Accesses a market given the market's eris_id_t
        SharedMember<Market> market(eris_id_t mid);

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
        template <class A> SharedMember<A> cloneAgent(A a);

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
        template <class G> SharedMember<G> cloneGood(G g);

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
        template <class M> SharedMember<M> cloneMarket(M m);

        /** Removes the given agent from this simulation.  Note that both Agent instances and
         * SharedMember<Agent> instances are automatically cast to eris_id_t when required, so
         * calling this method with those objects is acceptable (and indeed preferred).
         */
        void removeAgent(eris_id_t aid);
        /** Removes the given good from this simulation.  Note that both Good instances and
         * SharedMember<Good> instances are automatically cast to eris_id_t when required, so
         * calling this method with those objects is acceptable (and indeed preferred).
         */
        void removeGood(eris_id_t gid);
        /** Removes the given good from this simulation.  Note that both Good instances and
         * SharedMember<Good> instances are automatically cast to eris_id_t when required, so
         * calling this method with those objects is acceptable (and indeed preferred).
         */
        void removeMarket(eris_id_t mid);

        /** Provides read-only access to the map of the simulation's agents. */
        const AgentMap& agents();
        /** Provides read-only access to the map of the simulation's goods. */
        const GoodMap& goods();
        /** Provides read-only access to the map of the simulation's markets. */
        const MarketMap& markets();

    private:
        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        eris_id_t id_next = 1;
        AgentMap agent_map;
        GoodMap good_map;
        MarketMap market_map;
};

template <class A, typename... Args> SharedMember<A> Simulation::createAgent(const Args&... args) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(args...));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class A> SharedMember<A> Simulation::cloneAgent(A a) {
    // NB: Stored in a SM<Agent> rather than SM<A> ensures that A is an Agent subclass
    SharedMember<Agent> agent(new A(std::move(a)));
    insertAgent(agent);
    return agent; // Implicit recast back to SharedMember<A>
}

template <class G, typename... Args> SharedMember<G> Simulation::createGood(const Args&... args) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(args...));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class G> SharedMember<G> Simulation::cloneGood(G g) {
    // NB: Stored in a SM<Good> rather than SM<G> ensures that G is an Good subclass
    SharedMember<Good> good(new G(std::move(g)));
    insertGood(good);
    return good; // Implicit recast back to SharedMember<G>
}

template <class M, typename... Args> SharedMember<M> Simulation::createMarket(const Args&... args) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(args...));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}

template <class M> SharedMember<M> Simulation::cloneMarket(M m) {
    // NB: Stored in a SM<Market> rather than SM<M> ensures that M is an Market subclass
    SharedMember<Market> market(new M(std::move(m)));
    insertMarket(market);
    return market; // Implicit recast back to SharedMember<M>
}



}
