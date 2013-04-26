#include "Good.hpp"
#include <string>

Good::Good(std::string name, double increment) : good_name(name) {
    this->incr = increment < 0 ? 0 : increment;
}
Good::Good(double increment) : Good("", increment) {}

void Good::setId(eris_id_t new_id) {
    good_id = new_id;
    std::string foo = std::to_string(good_id);
    if (good_name == "") {
        good_name = "Good[" + std::to_string(new_id) + "]";
    }
}

eris_id_t Good::id() {
    return good_id;
}

std::string Good::name() {
    return good_name;
}
