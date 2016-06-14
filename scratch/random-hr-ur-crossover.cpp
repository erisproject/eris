#include <eris/random/truncated_normal_distribution.hpp>
#include <eris/random/rng.hpp>
#include <iostream>
#include <chrono>
#include <Eigen/Core>
#include <Eigen/SVD>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

using namespace Eigen;

// Calculates a linear formula for the hr/ur crossover.  This is done by dividing up the space from
// 0 to the hr_below_er_above threshold into equal increments for the left limit, then, for each
// value, we evaluate many HR and UR draws, to attempt to find the crossover.  The identified
// crossover is then kept, and finally, OLS is used to get the line.

// Shift the distribution parameters by this tiny amount so that internal calculations won't be
// trivial operations involving 1 or 0.
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

int main() {
    double mu = approx_zero, sigma = approx_one;

    auto &rng = eris::random::rng();

    // Busy loop to get CPU speed up
    double j = 3;
    for (int i = 0; i < 1000000000; i++) { j += 0.1; } if (j == 47) { std::cout << "j is wrong\n"; }

    // This number can be pretty small without problem: 0.02 seconds is still somewhere around half
    // a million draws
    constexpr double bench_time = 0.05;

    constexpr int num_left = 22; // The number of left divisions: total evalulations is one larger
    std::vector<double> left_values;
    for (int i = 0; i <= num_left; i++) left_values.push_back(i / double(num_left) * eris::random::detail::truncnorm_threshold<double>::hr_below_er_above);

    constexpr int num_right = 50;
    constexpr double right_delta_min = 0.25, right_delta_max = 0.75;
    std::vector<double> right_delta;
    for (int i = 0; i <= num_right; i++) right_delta.push_back(0.25 + i / double(num_right) * (right_delta_max - right_delta_min));

    VectorXd threshold_delta_r(num_left+1);
    MatrixXd outer_X_linear(num_left+1, 2);
    outer_X_linear.col(0).setOnes();

    for (size_t row = 0; row < left_values.size(); row++) {
        double left = left_values[row];
        std::vector<double> time_diff;
        time_diff.reserve(right_delta.size());
        for (double dright : right_delta) {
            double right = left + dright;

            const bool upper_tail = true;
            auto urtime = bench([&]() -> double {
                double inv2s2 = 0.5 / (sigma*sigma);
                double shift2 = (left - mu)*(left - mu);
                return eris::random::detail::truncnorm_rejection_uniform(rng, mu, left, right, inv2s2, shift2);
            }, bench_time);

            auto hrtime = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, left, right, upper_tail);
            }, bench_time);

            double ur_ns = urtime.second / urtime.first * 1e9, hr_ns = hrtime.second / hrtime.first * 1e9;
            time_diff.push_back(ur_ns - hr_ns);
        }

        // Now we'll go through at do a local linearization using a 7-near-point linear
        // approximation (ignoring the first and last three).  We'll finish by choosing the
        // threshold point that has the linear approximation fitted value closest to the middle
        // value.
        VectorXd y(7);
        MatrixXd X(7,2);
        Vector2d beta;
        X.col(0).setOnes();
        double best_predicted = std::numeric_limits<double>::quiet_NaN(),
               best_dist = std::numeric_limits<double>::infinity();
        int best_i = -1;
        for (int i = 3; i < num_right-4; i++) {
            for (int k = 0; k < 7; k++) {
                y[k] = time_diff[i-3+k];
                X(k,1) = right_delta[i-3+k];
            }
            beta = X.jacobiSvd(ComputeThinU | ComputeThinV).solve(y);
            double predicted_dright = -beta[0] / beta[1];
            double dist = std::fabs(predicted_dright - right_delta[i]);
            if (dist < best_dist) { best_dist = dist; best_predicted = predicted_dright; best_i = i; }
        }
        std::cout << "left=" << left << ": threshold = " << best_predicted << "\n";
        threshold_delta_r[row] = best_predicted;
        outer_X_linear(row, 1) = left;
    }

    MatrixXd outer_X_quadratic(num_left+1, 3);
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
    std::cout << "curve(" << final_beta_quadratic[0] << " + " << final_beta_quadratic[1] << "*x + " << final_beta_quadratic[2] << "*x^2, col=\"green\")\n";
}
