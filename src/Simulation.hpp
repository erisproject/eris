#pragma once
#include "types.hpp"
#include "Agent.hpp"
#include "Good.hpp"
#include <map>

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
        std::shared_ptr<Agent> agent(eris_id_t);
        std::shared_ptr<Good> good(eris_id_t);
        eris_id_t addAgent(Agent *a);
        eris_id_t addGood(Good *a);
        void removeAgent(eris_id_t aid);
        void removeGood(eris_id_t gid);
        AgentMap::iterator agents();
        AgentMap::iterator agentsEnd();
        GoodMap::iterator goods();
        GoodMap::iterator goodsEnd();
    private:
        eris_id_t agent_id_next = 1, good_id_next = 1;
        AgentMap agent_map;
        GoodMap good_map;
};

}
