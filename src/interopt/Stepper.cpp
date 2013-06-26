#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

Stepper::Stepper(double step, int increase_count) : step_(step), increase_(increase_count) {}

void Stepper::optimize() const {
    curr_up_ = should_increase();
}

void Stepper::apply() {
    if (curr_up_ == prev_up)
        ++same;
    else
        same = 1;

    update_step();

    double mult = curr_up_ ? (1 + step_) : 1.0 / (1 + step_);
    take_step(mult);

    prev_up = curr_up_;
}

void Stepper::update_step() {
    if (curr_up_ != prev_up) {
        // Changing directions, cut the step size
        step_ /= 2;
        if (step_ < min_step) step_ = min_step;
    }
    else if (same >= increase_) {
        step_ *= 2;

        same /= 2;
    }
}

} }
