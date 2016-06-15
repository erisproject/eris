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
    int increment = 500;
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

// Takes a vector of (value,time) pairs and uses local linear approximation to find the value point
// where time is 0.  Obviously value should start positive and end negative (or vice versa).  Throws
// an error if the best result is at the beginning or end.
//
// local_points is the size of the approximation, and must be odd.  Each point is considered using
// an OLS best-fit line through (i-n, i-(n-1), ..., i-1, i, i+1, ..., i+n) where n = local_approx/2
// (using integer division).
// The returned value is the root of the estimated line for the "i" whose root is closest to the
// actual value of i.
//
// If i ends up being the first or last admissable i (that is, first or last after ignoring the
// first local_approx/2), and error is raised: the proper root is probably outside the admissable
// range of values.
double zero_local_linear(const std::vector<std::pair<double,double>> &values, const int local_points) {

    VectorXd y(local_points);
    MatrixXd X(local_points, 2);
    Vector2d beta;
    X.col(0).setOnes();
    double best_predicted = std::numeric_limits<double>::quiet_NaN(),
           best_dist = std::numeric_limits<double>::infinity();
    int best_i = -1;
    for (int i = local_points / 2; i < int(values.size()) - local_points / 2; i++) {
        for (int k = 0; k < local_points; k++) {
            auto const &p = values[i-local_points/2+k];
            X(k,1) = p.first;
            y[k] = p.second;
        }
        beta = X.jacobiSvd(ComputeThinU | ComputeThinV).solve(y);
        double predicted_root = -beta[0] / beta[1];
        double dist = std::fabs(predicted_root - values[i].first);
        if (dist < best_dist) { best_dist = dist; best_predicted = predicted_root; best_i = i; }
    }
    if (best_i == local_points / 2) {
        std::cerr << "Error: first point found optimal; result unreliable.\n";
        throw std::runtime_error("Unreliable result");
    }
    else if (best_i == int(values.size()) - local_points / 2 - 1) {
        std::cerr << "Error: last point found optimal; result unreliable.\n";
        throw std::runtime_error("Unreliable result");
    }

    if (std::isnan(best_predicted)) throw std::logic_error("zero_local_linear: Could not compute any roots; perhaps data is too short?");

    return best_predicted;
}

// The distribution values.  The volatile versions should be used when the calculation time is meant
// to be included in the benchmark (because the compiler isn't allowed to optimize away the
// calculation).
double mu = approx_zero, sigma = approx_one;
volatile double mu_v = approx_zero, sigma_v = approx_one;

struct hr_ur {
    constexpr static double bench_time = 0.01; // Minimum number of seconds for each benchmark
    constexpr static int num_left = 15; // The number of left values: num_left-1 equal divisions of [0,max]
    constexpr static int local_points = 7; // The number of points to include around the current point.  Must be odd!
    constexpr static double backtrack = -0.02; // Start at the previous left optimum minus this
    constexpr static double increment = 0.001; // Take steps of this size, starting from prev+backtrack
    constexpr static double initial_start = 0.2; // Where we start looking (for left=0)
    constexpr static double initial_incr = 0.005; // Step size for left=0
    constexpr static int min_negs = 7; // Keep incrementing until we have found this many negative differences (must be at least local_points/2+1)
};

static_assert(hr_ur::local_points % 2 == 1, "hr_ur::local_points must be odd");
static_assert(hr_ur::min_negs > hr_ur::local_points / 2, "hr_ur::min_negs must be > hr_ur::local_points/2");

std::ostringstream line_R_code;

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
        for (double dright = start; num_neg < hr_ur::min_negs or time_diff.size() < hr_ur::local_points + 2; dright += incr) {
            constexpr bool upper_tail = true;
            const double right = left + dright;

            auto urtime = bench([&]() -> double {
                double inv2s2 = 0.5 / std::pow(sigma_v, 2);
                double shift2 = std::pow(left - mu_v, 2);
                return eris::random::detail::truncnorm_rejection_uniform(rng, mu, left, right, inv2s2, shift2);
            }, hr_ur::bench_time);

            auto hrtime = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, left, right, upper_tail);
            }, hr_ur::bench_time);

            // Calculate the average speed advantage of ur:
            double ns_diff = (hrtime.second / hrtime.first - urtime.second / urtime.first) * 1e9;
            time_diff.emplace_back(dright, ns_diff);
            if (ns_diff < 0) num_neg++;
            else num_neg = 0;
        }

        // Now we'll do a local linearization using a nearest-points linear approximation (ignoring
        // the first and last values) to find the optimal threshold for this left value.
        double best_predicted = zero_local_linear(time_diff, hr_ur::local_points);

        threshold_delta_r[row] = best_predicted;
        outer_X_linear(row, 1) = left;
    }

    MatrixXd outer_X_quadratic(left_values.size(), 3);
    outer_X_quadratic.leftCols(2) = outer_X_linear;
    outer_X_quadratic.col(2) = outer_X_linear.col(1).cwiseAbs2();

    Vector2d final_beta_linear = outer_X_linear.jacobiSvd(ComputeThinU | ComputeThinV).solve(threshold_delta_r);
    Vector3d final_beta_quadratic = outer_X_quadratic.jacobiSvd(ComputeThinU | ComputeThinV).solve(threshold_delta_r);

    line_R_code << "left <- cbind(c(" << left_values[0];
    for (size_t i = 1; i < left_values.size(); i++) {
        line_R_code << "," << left_values[i];
    }
    line_R_code << "))\nthresh <- cbind(c(" << threshold_delta_r[0];
    for (int i = 1; i < threshold_delta_r.size(); i++) {
        line_R_code << "," << threshold_delta_r[i];
    }
    line_R_code << "))\n";
    line_R_code << "plot(left, thresh)\n";
    line_R_code << "abline(a=" << final_beta_linear[0] << ", b=" << final_beta_linear[1] << ", col=\"blue\")\n";
    line_R_code << "curve(" << final_beta_quadratic[0] << " + " << final_beta_quadratic[1] << "*x + " << final_beta_quadratic[2] << "*x^2, col=\"red\", add=T)\n";

    return std::pair<Vector2d, Vector3d>(std::move(final_beta_linear), std::move(final_beta_quadratic));
}

struct er_hr {
    constexpr static double bench_time = 0.02; // Min seconds for each benchmark
    constexpr static double start = 0.5; // Start looking here
    constexpr static double incr = 0.002; // Increment by this amount each time
    constexpr static int local_points = 7; // Use linear approximation of this many points
    constexpr static int min_negs = 7; // Don't stop until we've found at least this many negative differences
};

static_assert(er_hr::local_points % 2 == 1, "hr_ur::local_points must be odd");
static_assert(er_hr::min_negs > er_hr::local_points / 2, "hr_ur::min_negs must be > hr_ur::local_points/2");

// Calculates the point where ER starts performing better than HR
// For values below er_lambda_below, we use full-lambda ER; above it, we use the approximation
double er_hr_threshold(double er_lambda_below) {
    auto &rng = eris::random::rng();
    std::vector<std::pair<double, double>> time_diff; // (left,time) pairs
    int num_neg = 0;
    for (double left = er_hr::start; num_neg < er_hr::min_negs or time_diff.size() < er_hr::local_points + 2;
            left += er_hr::incr) {

        constexpr bool upper_tail = true;
        const double right = std::numeric_limits<double>::infinity();

        auto hrtime = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, left, right, upper_tail);
                }, er_hr::bench_time);

        std::pair<long,double> ertime;
        if (left < er_lambda_below) {
            ertime = bench([&]() -> double {
                volatile double bd_v = left - mu_v;
                volatile double s = sigma_v;
                double proposal_param = 0.5 * (bd_v + sqrt(bd_v*bd_v + 4*s*s));
                return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, left, right, upper_tail, double(bd_v), proposal_param);
                }, er_hr::bench_time);
        }
        else {
            ertime = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, left, right, upper_tail, left - mu, left - mu);
                }, er_hr::bench_time);
        }

        // Calculate the average speed advantage of hr:
        double ns_diff = (ertime.second / ertime.first - hrtime.second / hrtime.first) * 1e9;
        time_diff.emplace_back(left, ns_diff);
        if (ns_diff < 0) num_neg++;
        else num_neg = 0;
    }

    return zero_local_linear(time_diff, er_hr::local_points);
}

struct er_er {
    constexpr static double bench_time = 0.05; // Min seconds for each benchmark
    constexpr static double start = 1.0; // Start looking here
    constexpr static double incr = 0.005; // Increment by this amount each time
    constexpr static int local_points = 7; // Use linear approximation of this many points
    constexpr static int min_negs = 12; // Don't stop until we've found at least this many negative differences
};

// Calculates the point where (approximated) ER(a) starts performing better than ER(lambda),
// where lambda is the exact (but expensive) calculation.
double er_er_threshold() {
    auto &rng = eris::random::rng();
    std::vector<std::pair<double, double>> time_diff; // (left,time) pairs
    int num_neg = 0;
    for (double left = er_er::start; num_neg < er_er::min_negs or time_diff.size() < er_er::local_points + 2;
            left += er_er::incr) {

        constexpr bool upper_tail = true;
        const double right = std::numeric_limits<double>::infinity();

        auto erltime = bench([&]() -> double {
                volatile double bd_v = left - mu_v;
                volatile double s = sigma_v;
                double proposal_param = 0.5 * (bd_v + sqrt(bd_v*bd_v + 4*s*s));
                return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, left, right, upper_tail, double(bd_v), proposal_param);
                }, er_er::bench_time);

        auto eratime = bench([&]() -> double {
                double bd = left - mu_v;
                return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, left, right, upper_tail, bd, bd);
                }, er_er::bench_time);

        // Calculate the average speed advantage of using lambda instead of the approximation:
        double ns_diff = (eratime.second / eratime.first - erltime.second / erltime.first) * 1e9;
        time_diff.emplace_back(left, ns_diff);
        if (ns_diff < 0) num_neg++;
        else num_neg = 0;
    }

    return zero_local_linear(time_diff, er_er::local_points);
}

int main() {

//    auto &rng = eris::random::rng();

    // Busy loop to get CPU speed up
    double j = 3;
    for (int i = 0; i < 500000000; i++) { j += 0.1; } if (j == 47) { std::cout << "j is wrong\n"; }

    std::cout << "Determining ER(a) vs ER(lambda) threshold..." << std::flush;
    auto erer = er_er_threshold();
    std::cout << " " << erer << std::endl;

    std::cout << "Determining ER/HR threshold..." << std::flush;
    auto erhr = er_hr_threshold(erer);
    std::cout << " " << erhr << std::endl;

    std::cout << "Determining HR/UR threshold line..." << std::flush;
    auto hrur = hr_ur_threshold(erhr);
    std::cout << " (l-r)/sigma = " << hrur.first[0] << " + " << hrur.first[1] << " l/sigma" << std::endl;

    std::cout << "\n\n\n";
    std::cout << "R code to plot HR/UR approximation:\n\n" << line_R_code.str() << "\n\n";

//    auto &final_beta_linear = hrur.first;
//    auto &final_beta_quadratic = hrur.second;

}
