#include <eris/interopt/InterStepper.hpp>

namespace eris { namespace interopt {

InterStepper::InterStepper(double step, int increase_count, int period, int period_offset)
    : stepper_(Stepper(step, increase_count)), period_(period), period_offset_(period_offset) {}

InterStepper::InterStepper(Stepper stepper, int period, int period_offset)
    : stepper_(stepper), period_(period), period_offset_(period_offset) {}

void InterStepper::interOptimize() {
    jump_ = should_jump();

    ++last_step_;

    if (jump_) {
        stepping_ = false;
    }
    else {
        stepping_ = last_step_ % period_ == period_offset_;
        if (stepping_) {
            curr_up_ = should_increase();
        }
    }
}

void InterStepper::interApply() {
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
bool InterStepper::should_jump() { return false; }

} }
