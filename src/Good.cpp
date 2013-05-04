#include "Good.hpp"
#include <exception>
#include <string>
namespace eris {

Good::Good(std::string good_name) : name(good_name) {}

Good::Continuous::Continuous(std::string name) : Good(name) {}

double Good::Continuous::increment() { return 0.0; }

Good::Discrete::Discrete(std::string name, double increment) : Good(name) {
    setIncrement(increment);
}

void Good::Discrete::setIncrement(double increment) {
    if (increment <= 0) throw std::invalid_argument(std::to_string(increment));
    incr = increment;
}

double Good::Discrete::increment() { return incr; }

}
