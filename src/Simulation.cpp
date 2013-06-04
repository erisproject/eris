#include <eris/Simulation.hpp>

namespace eris {

SharedMember<Agent> Simulation::agent(eris_id_t aid) {
    return agent_map.at(aid);
}

SharedMember<Good> Simulation::good(eris_id_t gid) {
    return good_map.at(gid);
}

SharedMember<Market> Simulation::market(eris_id_t mid) {
    return market_map.at(mid);
}

// Assign an ID, set it, store the simulator, and insert into the agent map
void Simulation::insertAgent(const SharedMember<Agent> &a) {
    a->_id = id_next++;
    a->simulation = shared_from_this();
    agent_map.insert(std::make_pair(a->id(), a));
}
// Assign an ID, set it, store the simulator, and insert into the good map
void Simulation::insertGood(const SharedMember<Good> &g) {
    g->_id = id_next++;
    g->simulation = shared_from_this();
    good_map.insert(std::make_pair(g->id(), g));
}
// Assign an ID, set it, store the simulator, and insert into the agent map
void Simulation::insertMarket(const SharedMember<Market> &m) {
    m->_id = id_next++;
    m->simulation = shared_from_this();
    market_map.insert(std::make_pair(m->id(), m));
}

void Simulation::removeAgent(eris_id_t aid)  { agent_map.erase(aid);  }
void Simulation::removeGood(eris_id_t gid)   { good_map.erase(gid);   }
void Simulation::removeMarket(eris_id_t gid) { market_map.erase(gid); }

const Simulation::AgentMap& Simulation::agents()   { return agent_map;  }
const Simulation::GoodMap& Simulation::goods()     { return good_map;   }
const Simulation::MarketMap& Simulation::markets() { return market_map; }

}
