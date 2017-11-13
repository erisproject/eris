#include <eris/Good.hpp>

namespace eris {

SharedMember<Member> Good::sharedSelf() const { return simGood(id()); }

Good::operator std::string() const {
    return "Good[" + name + (name.empty() ? "" : ", id=") + std::to_string(id()) + "]";
}

}
