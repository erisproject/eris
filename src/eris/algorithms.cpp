#include <eris/algorithms.hpp>
#include <cmath>

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

// Relative left midpoint position.  Equal to \f$2 - \varphi\f$.
const double sps_left_mid_ = 1.5 - std::sqrt(1.25);
// Relative right midpoint position.  Equal to \f$\varphi - 1\f$.
const double sps_right_mid_ = 1 - sps_left_mid_;

double sps_(
        const std::function<double(const double &)> &f,
        const double &left, const double &midleft, const double &midright, const double &right,
        const double &fl, const double &fml, const double &fmr, const double &fr,
        const double &tolerance) {
    if ((right - left) / std::max(std::fabs(left), std::fabs(right)) <= tolerance) {
        // Prefer the end-points for ties (the max might legitimate be an end-point), and prefer
        // left over right (for no particularly good reason).
        return fl >= fml && fl >= fmr && fl >= fr ? left :
            fr >= fmr && fr >= fml ? right :
            fml >= fmr ? midleft : midright;
    }

    if (fml >= fmr) {
        // midleft is the higher point, so we can exclude everything right of midright.
        // midright -> new right
        // midleft -> new midright
        // and calculate the new midleft position and evaluate it for the next iteration
        const double newmidleft = left + (midright - left) * sps_left_mid_;
        return sps_(f, left, newmidleft, midleft, midright, fl, f(newmidleft), fml, fmr, tolerance);
    }
    else {
        // midright is higher, so exclude everything left of midleft.
        // midleft -> new left
        // midright -> new midleft
        // and calculated the new midright position and evaluate for the next iteration
        const double newmidright = midleft + (right - midleft) * sps_right_mid_;
        return sps_(f, midleft, midright, newmidright, right, fml, fmr, f(newmidright), fr, tolerance);
    }
}

double single_peak_search(
        const std::function<double(const double &)> &f,
        const double &left, const double &right,
        const double &tolerance) {

    const double size = right - left;
    const double midleft = left + sps_left_mid_*size;
    const double midright = left + sps_right_mid_*size;

    return sps_(f, left, midleft, midright, right, f(left), f(midleft), f(midright), f(right), tolerance);
}

}
