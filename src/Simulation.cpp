#include "Simulation.hpp"

namespace eris {

std::shared_ptr<Agent> Simulation::agent(eris_id_t aid) {
    return agent_map.at(aid);
}

std::shared_ptr<Good> Simulation::good(eris_id_t gid) {
    return good_map.at(gid);
}

// Stores the passed-in Agent.  Note that this Agent pointer must be created
// using new, i.e. do not pass a pointer to an Agent object that will be
// automatically destroyed!
eris_id_t Simulation::addAgent(Agent *a) {
    eris_id_t id = agent_id_next++;
    a->simulator = shared_from_this();
    a->id = id;
    agent_map.insert(std::make_pair(id, std::shared_ptr<Agent>(a)));
    return id;
}

// Stores the passed-in Good.  Note that this Good pointer must be created
// using new, i.e. you cannot pass a pointer to a heap variable (because it
// would be destroyed).
eris_id_t Simulation::addGood(Good *g) {
    eris_id_t id = good_id_next++;
    good_map.insert(std::make_pair(id, std::shared_ptr<Good>(g)));
    return id;
}

void Simulation::removeAgent(eris_id_t aid) {
    agent_map.erase(aid);
}

void Simulation::removeGood(eris_id_t gid) {
    good_map.erase(gid);
}

AgentMap::iterator Simulation::agents() {
    return agent_map.begin();
}
AgentMap::iterator Simulation::agentsEnd() {
    return agent_map.end();
}

GoodMap::iterator Simulation::goods() {
    return good_map.begin();
}
GoodMap::iterator Simulation::goodsEnd() {
    return good_map.end();
}

}
