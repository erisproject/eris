#include <cstdint>
#include <map>

typedef uint64_t eris_id_t;

class Good;
class Agent;
typedef std::map<eris_id_t, Good> GoodMap;
typedef std::map<eris_id_t, Agent> AgentMap;

typedef std::map<eris_id_t, double> Bundle;
