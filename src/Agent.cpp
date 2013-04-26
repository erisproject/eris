#include "Agent.hpp"

void Agent::setId(eris_id_t new_id) {
    agent_id = new_id;
}

eris_id_t Agent::id() {
    return agent_id;
}

void Agent::setSim(Eris *s) {
    simulator = s;
}
