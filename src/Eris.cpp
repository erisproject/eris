#include "Eris.hpp"
#include "Agent.hpp"
#include "Good.hpp"

Agent& Eris::agent(eris_id_t aid) {
    return agent_map.at(aid);
}

Good& Eris::good(eris_id_t gid) {
    return good_map.at(gid);
}

eris_id_t Eris::addAgent(Agent a) {
    eris_id_t id = agent_id_next++;
    a.setId(id);
    a.setSim(this);
    agent_map.insert(make_pair(id, a));
    return id;
}

// Store a copy of the passed-in Good
eris_id_t Eris::addGood(Good g) {
    eris_id_t id = good_id_next++;
    g.setId(id);
    good_map.insert(make_pair(id, g));
    return id;
}

void Eris::removeAgent(eris_id_t aid) {
    agent_map.erase(aid);
}

void Eris::removeGood(eris_id_t gid) {
    good_map.erase(gid);
}

AgentMap::iterator Eris::agents() {
    return agent_map.begin();
}
AgentMap::iterator Eris::agentsEnd() {
    return agent_map.end();
}

GoodMap::iterator Eris::goods() {
    return good_map.begin();
}
GoodMap::iterator Eris::goodsEnd() {
    return good_map.end();
}
