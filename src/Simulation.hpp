#pragma once
#include "types.hpp"
#include "Agent.hpp"
#include "Good.hpp"
#include <map>
#include <type_traits>
#include <exception>

namespace eris {

typedef std::map<eris_id_t, SharedGood<Good>> GoodMap;
typedef std::map<eris_id_t, SharedAgent<Agent>> AgentMap;

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
        SharedAgent<Agent> agent(eris_id_t aid);
        SharedGood<Good> good(eris_id_t gid);


        template <class A> SharedAgent<A> addAgent(A a);
        template <class G> SharedGood<G> addGood(G g);
        // void remove(Agent *a);
        // void remove(Good *g);
        void removeAgent(eris_id_t aid);
        void removeGood(eris_id_t gid);
        void removeAgent(const Agent &a);
        void removeGood(const Good &g);
        const AgentMap agents();
        const GoodMap goods();

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
        void insert(const SharedAgent<Agent> &agent);
        void insert(const SharedGood<Good> &good);
        eris_id_t agent_id_next = 1, good_id_next = 1;
        AgentMap agent_map;
        GoodMap good_map;
};

// Copies and stores the passed in Agent and returns a shared pointer to it.  Will throw a
// Simulation::already_owned exception if the agent belongs to another Simulation, and a
// Simulation::already_added exception if the agent has already been added to this Simulation.
template <class A> SharedAgent<A> Simulation::addAgent(A a) {
    SharedAgent<Agent> agent(new A(std::move(a)));
    insert(agent);
    return SharedAgent<A>(agent);
}

// Copies and stores the passed-in Good, and returned a shared pointer to it.
template <class G> SharedGood<G> Simulation::addGood(G g) {
    SharedGood<Good> good(new G(std::move(g)));
    insert(good);
    return SharedGood<G>(good);
}




}
