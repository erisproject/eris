#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

Stepper::Stepper(double step, int increase_count, int period)
    : stepper_(eris::Stepper(step, increase_count)), period_(period) {}

void Stepper::optimize() const {
    stepping_ = ++last_step_ >= period_;
    if (stepping_) {
        last_step_ = 0;
        curr_up_ = should_increase();
    }
}

void Stepper::apply() {
    if (stepping_) take_step(stepper_.step(curr_up_));
}

JumpStepper::JumpStepper(double step, int increase_count, int period) : Stepper(step, increase_count, period) {}

void JumpStepper::optimize() const {
    jump_ = should_jump();

    if (!jump_)
        Stepper::optimize();
}

void JumpStepper::apply() {
    if (jump_) {
        stepper_.same = 0;
        take_jump();
    }
    else {
        Stepper::apply();
    }
}

} }
