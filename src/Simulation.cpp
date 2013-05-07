#include "Simulation.hpp"

namespace eris {

Simulation::SharedObject<Agent> Simulation::agent(eris_id_t aid) {
    return agent_map.at(aid);
}

Simulation::SharedObject<Good> Simulation::good(eris_id_t gid) {
    return good_map.at(gid);
}

// Assign an ID, set it, store the simulator, and instead into the agent map
void Simulation::insertAgent(const Simulation::SharedObject<Agent> &a) {
    a->_id = agent_id_next++;
    a->simulator = shared_from_this();
    agent_map.insert(std::make_pair(a->id(), a));
}
// Assign an ID, set it, store the simulator, and instead into the good map
void Simulation::insertGood(const Simulation::SharedObject<Good> &g) {
    g->_id = good_id_next++;
    g->simulator = shared_from_this();
    good_map.insert(std::make_pair(g->id(), g));
}

void Simulation::removeAgent(eris_id_t aid) {
    agent_map.erase(aid);
}

void Simulation::removeGood(eris_id_t gid) {
    good_map.erase(gid);
}

const Simulation::AgentMap Simulation::agents() {
    return agent_map;
}

const Simulation::GoodMap Simulation::goods() {
    return good_map;
}

}
