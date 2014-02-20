#include <eris/algorithms.hpp>
namespace eris {

Stepper::Stepper(double step, int increase_count, double min_step, double max_step, bool rel_steps)
    : increase(increase_count), min_step(min_step), max_step(max_step), relative_steps(rel_steps), step_size(step) {}

double Stepper::step(bool up) {
    bool first_time = same == 0;
    if (up == prev_up)
        ++same;
    else
        same = 1;

    bool around_min = false;

    if (up != prev_up and not first_time) {
        // Changing directions, cut the step size in half

        if (step_size == min_step) {
            // If we're already at the minimum, and we're changing direction, we're just oscillating
            // around the optimal value, so set that flag.
            around_min = true;
        }
        else {
            step_size /= 2;
            if (step_size < min_step) step_size = min_step;
        }
    }
    else if (same >= increase and step_size < max_step) {
        // We've taken several steps in the same direction, so double the step size
        step_size *= 2;

        if (step_size > max_step) step_size = max_step;

        // We just doubled the step size, so, in terms of the new step size, only half of the
        // previous steps count as increasing steps.  This could be slightly off if we had to
        // correct to the max_step size, but in that case the value of same won't matter anyway,
        // since we won't be increasing step size any more.
        same /= 2;
    }

    if (around_min)
        oscillating_min++;
    else
        oscillating_min = 0;

    prev_up = up;

    if (not relative_steps) return up ? step_size : -step_size;
    else if (up) return 1 + step_size;
    else return 1.0 / (1 + step_size);
}

}
