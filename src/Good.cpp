#include "Good.hpp"
#include <string>

Good::Good(std::string good_name) : name(good_name) {}

Good::Continuous::Continuous(std::string name) : Good(name) {}

double Good::Continuous::increment() { return 0.0; }

Good::Discrete::Discrete(std::string name, double increment) : Good(name) {
    setIncrement(increment);
}

// FIXME: throw an exception for non-positive increments?
void Good::Discrete::setIncrement(double increment) {
    if (increment <= 0) throw "FIXME-increment non-positive";
    incr = increment;
}

double Good::Discrete::increment() { return incr; }

