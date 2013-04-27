#include <cstdint>
#include <map>
#include <memory>

typedef uint64_t eris_id_t;

class Good;
class Agent;
typedef std::map<eris_id_t, std::shared_ptr<Good>> GoodMap;
typedef std::map<eris_id_t, std::shared_ptr<Agent>> AgentMap;
