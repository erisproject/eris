#include "Good.hpp"
#include <string>

Good::Good(std::string name, double increment) {
    setName(name);
    setIncrement(increment);
}

std::string Good::name() {
    return good_name;
}

// FIXME: throw an exception for negative increments?
void Good::setIncrement(double increment) {
    incr = increment < 0 ? 0 : increment;
}

void Good::setName(std::string name) {
    good_name = name;
}
