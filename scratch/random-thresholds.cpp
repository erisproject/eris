#include <eris/random/truncated_normal_distribution.hpp>
#include <eris/random/rng.hpp>
#include <iostream>
#include <chrono>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#include <Eigen/Core>
#include <Eigen/SVD>
#pragma GCC diagnostic pop

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

using namespace Eigen;

// Calculates various thresholds for deciding among truncated normal algorithms.

// Shift the distribution parameters by this tiny amount so that internal calculations won't be
// trivial operations involving 1 or 0, but the numbers are small enough as to not affect the
// results.
constexpr double approx_zero = -1e-300, approx_one = 1 + 1e-12;

// Accumulate results here, so that they can't be compiled away:
double garbage = 0.0;
// Runs code for at least the given time; returns the number of times run, and the total runtime.
std::pair<long, double> bench(std::function<double()> f, double at_least = 0.25) {
    auto start = clk::now();
    std::pair<long, double> results;
    int increment = 50;
    do {
        increment *= 2;
        for (int i = 0; i < increment; i++) {
            garbage += f();
        }
        results.second = dur(clk::now() - start).count();
        results.first += increment;
    } while (results.second < at_least);

    return results;
}

// This number can be pretty small without problem: 0.1 seconds is still somewhere around two
// million draws, which is far more than enough to invoke a LLN.
constexpr double bench_time = 0.02;

// The distribution values.  The volatile versions should be used when the calculation time is meant
// to be included in the benchmark (because the compiler isn't allowed to optimize away the
// calculation).
double mu = approx_zero, sigma = approx_one;
volatile double mu_v = approx_zero, sigma_v = approx_one;

struct hr_ur {
    constexpr static int num_left = 26; // The number of left values: num_left-1 equal divisions of [0,max]
    constexpr static int local_points = 7; // The number of points to include around the current point.  Must be odd!
    constexpr static double backtrack = -0.01; // Start at the previous left optimum minus this
    constexpr static double increment = 0.001; // Take steps of this size, starting from prev+backtrack
    constexpr static double initial_start = 0.2; // Where we start looking (for left=0)
    constexpr static double initial_incr = 0.005; // Step size for left=0
    constexpr static int min_negs = 7; // Keep incrementing until we have found this many negative differences (must be at least local_points/2+1)
};

static_assert(hr_ur::local_points % 2 == 1, "hr_ur::local_points must be odd");
static_assert(hr_ur::min_negs > hr_ur::local_points / 2, "hr_ur::min_negs must be > hr_ur::local_points/2");

// Calculates a linear approximation to the threshold value for HR-vs-UR decisions.  This isn't
// actually a perfectly linear function, but as long as the ER-vs-HR threshold isn't too high, the
// linear approximation is a pretty decent fit.
//
// The returned pair has the coefficients of the linear (.first) and quadratic (.second)
// approximations.
//
// `er_begins` is the end-point, i.e. where the HR-UR threshold stops mattering.  We construct the
// approximation by getting evenly-distributed observations from 0 to `er_begins`.
std::pair<Vector2d, Vector3d> hr_ur_threshold(double er_begins) {
    // For the first observation at left=0, we have a fairly broad range of values; for subsequent
    // values we can start at the previous observation and move upwards (this function is
    // monotonic).

    auto &rng = eris::random::rng();

    std::vector<double> left_values;
    for (int i = 0; i < hr_ur::num_left; i++) left_values.emplace_back(i / double(hr_ur::num_left-1) * er_begins);

    VectorXd threshold_delta_r(hr_ur::num_left);
    MatrixXd outer_X_linear(hr_ur::num_left, 2);
    outer_X_linear.col(0).setOnes();

    // First case (left=0): we start at initial_start, then increment by initial_incr, and repeat
    // this until we have at least local_points excess values (we need local_points/2 just to do the
    // linearization, but the extra points should ensure that we are definitely in the right
    // section).
    for (size_t row = 0; row < left_values.size(); row++) {
        double left = left_values[row];
        const double start = row == 0 ? hr_ur::initial_start : threshold_delta_r[row-1] + hr_ur::backtrack;
        const double incr = row == 0 ? hr_ur::initial_incr : hr_ur::increment;

        int num_neg = 0;
        std::vector<std::pair<double, double>> time_diff; // (dright,time) pairs
        for (double dright = start; num_neg < hr_ur::min_negs or time_diff.size() < 2*hr_ur::min_negs + 1; dright += incr) {
            constexpr bool upper_tail = true;
            const double right = left + dright;

            auto urtime = bench([&]() -> double {
                double inv2s2 = 0.5 / std::pow(sigma_v, 2);
                double shift2 = std::pow(left - mu_v, 2);
                return eris::random::detail::truncnorm_rejection_uniform(rng, mu, left, right, inv2s2, shift2);
            }, bench_time);

            auto hrtime = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, left, right, upper_tail);
            }, bench_time);

            // Calculate the average speed advantage of ur:
            double ns_diff = (hrtime.second / hrtime.first - urtime.second / urtime.first) * 1e9;
            time_diff.emplace_back(dright, ns_diff);
            if (ns_diff < 0) num_neg++;
            else num_neg = 0;
        }

        // Now we'll go through at do a local linearization using a nearest-points linear
        // approximation (ignoring the first and last values).  We'll finish by choosing the
        // threshold point that has the linear approximation fitted value closest to the middle
        // value, but raise an error if that value is the first or last point evaluated (because
        // that probably means the true value is beyond the evaluation end-point).
        VectorXd y(hr_ur::local_points);
        MatrixXd X(hr_ur::local_points, 2);
        Vector2d beta;
        X.col(0).setOnes();
        double best_predicted = std::numeric_limits<double>::quiet_NaN(),
               best_dist = std::numeric_limits<double>::infinity();
        int best_i = -1;
        for (int i = hr_ur::local_points / 2; i < int(time_diff.size()) - hr_ur::local_points / 2; i++) {
            for (int k = 0; k < hr_ur::local_points; k++) {
                auto const &p = time_diff[i-hr_ur::local_points/2+k];
                X(k,1) = p.first;
                y[k] = p.second;
            }
            beta = X.jacobiSvd(ComputeThinU | ComputeThinV).solve(y);
            double predicted_dright = -beta[0] / beta[1];
            double dist = std::fabs(predicted_dright - time_diff[i].second);
            if (dist < best_dist) { best_dist = dist; best_predicted = predicted_dright; best_i = i; }
        }
        if (best_i == hr_ur::local_points / 2) {
            std::cerr << "Error: first point found optimal; result unreliable.\n";
            throw std::runtime_error("Unreliable result");
        }
        else if (best_i == int(time_diff.size()) - hr_ur::local_points / 2 - 1) {
            std::cerr << "Error: last point found optimal; result unreliable.\n";
            throw std::runtime_error("Unreliable result");
        }

        std::cout << "HR/UR: for left = " << left << " found threshold " << best_predicted << std::endl;
        threshold_delta_r[row] = best_predicted;
        outer_X_linear(row, 1) = left;
    }

    MatrixXd outer_X_quadratic(left_values.size(), 3);
    outer_X_quadratic.leftCols(2) = outer_X_linear;
    outer_X_quadratic.col(2) = outer_X_linear.col(1).cwiseAbs2();

    Vector2d final_beta_linear = outer_X_linear.jacobiSvd(ComputeThinU | ComputeThinV).solve(threshold_delta_r);
    Vector3d final_beta_quadratic = outer_X_quadratic.jacobiSvd(ComputeThinU | ComputeThinV).solve(threshold_delta_r);

    std::cout << "\n\nFinal result:\n    linear (R-L = a + b L): (a b) = " << final_beta_linear.transpose() << "\n\n";
    std::cout << "\n\nFinal result:\n    quadratic (R-L = a + b L + c L^2): (a b c) = " << final_beta_quadratic.transpose() << "\n\n";
    std::cout << "R code to plot values/line:\n\n";
    std::cout << "left <- cbind(c(" << left_values[0];
    for (size_t i = 1; i < left_values.size(); i++) {
        std::cout << "," << left_values[i];
    }
    std::cout << "))\nthresh <- cbind(c(" << threshold_delta_r[0];
    for (int i = 1; i < threshold_delta_r.size(); i++) {
        std::cout << "," << threshold_delta_r[i];
    }
    std::cout << "))\n";
    std::cout << "plot(left, thresh)\n";
    std::cout << "abline(a=" << final_beta_linear[0] << ", b=" << final_beta_linear[1] << ", col=\"blue\")\n";
    std::cout << "curve(" << final_beta_quadratic[0] << " + " << final_beta_quadratic[1] << "*x + " << final_beta_quadratic[2] << "*x^2, col=\"red\", add=T)\n";

    return std::pair<Vector2d, Vector3d>(std::move(final_beta_linear), std::move(final_beta_quadratic));
}

int main() {

//    auto &rng = eris::random::rng();

    // Busy loop to get CPU speed up
    double j = 3;
    for (int i = 0; i < 500000000; i++) { j += 0.1; } if (j == 47) { std::cout << "j is wrong\n"; }

    auto hrur = hr_ur_threshold(0.55);
//    auto &final_beta_linear = hrur.first;
//    auto &final_beta_quadratic = hrur.second;

}
