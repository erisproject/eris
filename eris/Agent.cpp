#include <eris/Agent.hpp>

namespace eris {

SharedMember<Member> Agent::sharedSelf() const { return simAgent(id()); }

}
