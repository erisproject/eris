#include <eris/algorithms.hpp>
#include <cmath>
#include <algorithm>
#include <boost/math/constants/constants.hpp>

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

// Relative right midpoint position.  Equal to \f$\varphi - 1\f$.
constexpr double sps_right_mid_ = boost::math::constants::phi<double>() - 1;
// Relative left midpoint position.  Equal to 1 minus `sps_right_mid_`, which also equals 2 - phi.
constexpr double sps_left_mid_ = 1 - sps_right_mid_;

double single_peak_search(
        const std::function<double(const double &)> &f,
        double left,
        double right,
        double tol_rel,
        double tol_abs) {

    if (tol_rel < 0) tol_rel = 0;
    if (tol_abs < 0) tol_abs = 0;

    double midleft = left + sps_left_mid_*(right - left);
    double midright = left + sps_right_mid_*(right - left);
    double fl = f(left), fml = f(midleft), fmr = f(midright), fr = f(right);

    while (
        tol_abs < right - left
            and
        tol_rel < (right - left) / std::max(std::fabs(left), std::fabs(right))
    ) {

        if (fml >= fmr) {
            // midleft is the higher point, so we can exclude everything right of midright.
            right = midright; fr = fmr;
            midright = midleft; fmr = fml;
            midleft = left + (right - left) * sps_left_mid_;
            fml = f(midleft);
            // If the midpoint is closer to the endpoint than is numerically distinguishable, finish:
            if (midleft == left) break;
        }
        else {
            // midright is higher, so exclude everything left of midleft.
            left = midleft; fl = fml;
            midleft = midright; fml = fmr;
            midright = left + (right - left) * sps_right_mid_;
            fmr = f(midright);
            // If the midpoint is closer to the endpoint than is numerically distinguishable, finish:
            if (midright == right) break;
        }

        // Sometimes, we can run into numerical instability that results in midleft > midright,
        // particular when the optimum is close to 0.  If we encounter that, swap midleft and
        // midright.
        if (midleft > midright) {
            std::swap(midleft, midright);
            std::swap(fml, fmr);
        }
    }

    // Prefer the end-points for ties (the max might legitimate be an end-point), and prefer
    // left over right (for no particularly good reason).
    return fl >= fml && fl >= fmr && fl >= fr ? left :
        fr >= fmr && fr >= fml ? right :
        fml >= fmr ? midleft : midright;
}

}
