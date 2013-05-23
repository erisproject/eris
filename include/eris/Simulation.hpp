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

/* This class is at the centre of an Eris economy model; it keeps track of all
 * of the agents currently in the economy, all of the goods currently available
 * in the economy, and the interaction mechanisms (e.g. markets).  Note that
 * all of these can change from one period to the next.  It's also responsible
 * for dispatching interactions (e.g. letting markets operate) and any
 * iteration-sensitive agent events (e.g. aging/dying/etc.).
 *
 * In short, this is the central piece of the Eris framework that dictates how
 * all the other pieces interact.
 */
class Simulation : public std::enable_shared_from_this<Simulation> {
    public:
        //Simulation();
        virtual ~Simulation() = default;

        typedef std::map<eris_id_t, SharedMember<Good>> GoodMap;
        typedef std::map<eris_id_t, SharedMember<Agent>> AgentMap;
        typedef std::map<eris_id_t, SharedMember<Market>> MarketMap;

        SharedMember<Agent> agent(eris_id_t aid);
        SharedMember<Good> good(eris_id_t gid);
        SharedMember<Market> market(eris_id_t mid);

        template <class A> SharedMember<A> addAgent(A a);
        template <class G> SharedMember<G> addGood(G g);
        template <class M> SharedMember<M> addMarket(M m);
        void removeAgent(eris_id_t aid);
        void removeGood(eris_id_t gid);
        void removeMarket(eris_id_t mid);
        void removeAgent(const Agent &a);
        void removeGood(const Good &g);
        void removeMarket(const Market &m);
        const AgentMap agents();
        const GoodMap goods();
        const MarketMap markets();

        class already_owned : public std::invalid_argument {
            public:
                already_owned(const std::string &objType, std::shared_ptr<Simulation> sim) :
                    std::invalid_argument(objType + " belongs to another Simulation"),
                    simulation(sim) {}
                const std::shared_ptr<Simulation> simulation;
        };
        class already_added : public std::invalid_argument {
            public:
                already_added(const std::string &objType) : std::invalid_argument(objType + " already belongs to this Simulation") {}
        };


    private:
        void insertAgent(const SharedMember<Agent> &agent);
        void insertGood(const SharedMember<Good> &good);
        void insertMarket(const SharedMember<Market> &market);
        eris_id_t id_next = 1;
        AgentMap agent_map;
        GoodMap good_map;
        MarketMap market_map;
};

// Copies and stores the passed in Agent and returns a shared pointer to it.  Will throw a
// Simulation::already_owned exception if the agent belongs to another Simulation, and a
// Simulation::already_added exception if the agent has already been added to this Simulation.
template <class A> SharedMember<A> Simulation::addAgent(A a) {
    // This will fail if A isn't an Agent (sub)class:
    SharedMember<Agent> agent(new A(std::move(a)));
    insertAgent(agent);
    return agent;
}

// Copies and stores the passed-in Good, and returned a shared pointer to it.
template <class G> SharedMember<G> Simulation::addGood(G g) {
    // This will fail if G isn't a Good (sub)class:
    SharedMember<Good> good(new G(std::move(g)));
    insertGood(good);
    return good;
}

// Copies and stores the passed-in Market, and returned a shared pointer to it.
template <class M> SharedMember<M> Simulation::addMarket(M g) {
    // This will fail if M isn't a Market (sub)class:
    SharedMember<Market> market(new M(std::move(g)));
    insertMarket(market);
    return market;
}




}
