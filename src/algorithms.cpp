#include <eris/algorithms.hpp>
namespace eris {

Stepper::Stepper(double step, int increase_count, double min_step, bool rel_steps)
    : increase(increase_count), min_step(min_step), relative_steps(rel_steps), step_size(step) {}

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
        // previous steps count as increasing steps:
        same /= 2;
    }

    prev_up = up;

    if (not relative_steps) return up ? step_size : -step_size;
    else if (up) return 1 + step_size;
    else return 1.0 / (1 + step_size);
}

}
