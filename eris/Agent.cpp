#include <eris/Agent.hpp>

namespace eris {

SharedMember<Member> Agent::sharedSelf() const { return simAgent(id()); }

Agent::operator std::string() const { return "Agent[" + std::to_string(id()) + "]"; }

}
