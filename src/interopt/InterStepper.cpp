#include <eris/interopt/InterStepper.hpp>

namespace eris { namespace interopt {

InterStepper::InterStepper(double step, int increase_count, int period)
    : stepper_(Stepper(step, increase_count)), period_(period) {}

void InterStepper::optimize() const {
    jump_ = should_jump();

    ++last_step_;

    if (jump_) {
        stepping_ = false;
        last_step_ = 0;
    }
    else {
        stepping_ = last_step_ >= period_;
        if (stepping_) {
            last_step_ = 0;
            curr_up_ = should_increase();
        }
    }
}

void InterStepper::apply() {
    if (jump_) {
        stepper_.same = 0;
        take_jump();
    }
    else if (stepping_) {
        take_step(stepper_.step(curr_up_));
    }
}

// Stubs
void InterStepper::take_jump() {}
bool InterStepper::should_jump() const { return false; }

} }
