#include <eris/Good.hpp>
#include <exception>
#include <string>
namespace eris {

Good::Good(std::string name) : name(name) {}

Good::Continuous::Continuous(std::string name) : Good(name) {}

double Good::Continuous::increment() { return 0.0; }

Good::Discrete::Discrete(double increment, std::string name) : Good(name) {
    setIncrement(increment);
}

void Good::Discrete::setIncrement(double increment) {
    if (increment <= 0) throw std::invalid_argument(std::to_string(increment));
    incr_ = increment;
}

double Good::Discrete::increment() { return incr_; }

SharedMember<Member> Good::sharedSelf() const { return simGood(id()); }

}
