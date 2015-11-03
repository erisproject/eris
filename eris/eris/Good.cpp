#include <eris/Good.hpp>

namespace eris {

Good::Good(std::string name) : name(std::move(name)) {}
Good::Continuous::Continuous(std::string name) : Good(std::move(name)) {}
Good::Discrete::Discrete(std::string name) : Good(std::move(name)) {}

SharedMember<Member> Good::sharedSelf() const { return simGood(id()); }

}
