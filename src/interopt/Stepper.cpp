#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

Stepper::Stepper(double step, int increase_count)
    : stepper_(eris::Stepper(step, increase_count)) {}

void Stepper::optimize() const {
    curr_up_ = should_increase();
}

void Stepper::apply() {
    take_step(stepper_.step(curr_up_));
}

} }
