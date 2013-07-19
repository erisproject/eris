#include <eris/algorithms.hpp>
namespace eris {

Stepper::Stepper(double step, int increase_count, double min_step)
    : increase(increase_count), min_step(min_step), step_size(step) {}

double Stepper::step(bool up) {
    bool first_time = same == 0;
    if (up == prev_up)
        ++same;
    else
        same = 1;

    if (up != prev_up and not first_time) {
        // Changing directions, cut the step size in half
        step_size /= 2;
        if (step_size < min_step) step_size = min_step;
    }
    else if (same >= increase) {
        // We've taken several steps in the same direction, so double the step size
        step_size *= 2;
        // We just doubled the step size, so, in terms of the new step size, only half of the
        // previous steps count:
        same /= 2;
    }

    double mult = up ? (1 + step_size) : 1.0 / (1 + step_size);

    prev_up = up;

    return mult;
}

}
