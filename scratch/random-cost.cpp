#include <eris/random/rng.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <iostream>
#include <functional>
#include <chrono>
#include <regex>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

// Shift the distribution parameters by this tiny amount so that internal calculations won't be
// trivial operations involving 1 or 0.
constexpr double approx_zero = -1e-300, approx_one = 1 + 1e-12;

struct benchmark {
    const double left, right;
    bool normal{false}, halfnormal{false}, uniform{false}, exponential{false};
    struct {
        std::pair<long, double> selected, normal, halfnormal, uniform, exponential, expo_approx, expo_cost;
    } timing;
    benchmark(double l, double r)
        : left{l}, right{r}
    {}
};

std::string double_str(double d, unsigned precision = std::numeric_limits<double>::max_digits10) {
    double round_trip;
    for (unsigned prec : {precision-2, precision-1}) {
        std::stringstream ss;
        ss.precision(prec);
        ss << d;
        ss >> round_trip;
        if (round_trip == d) { ss << d; return ss.str(); }
    }
    std::stringstream ss;
    ss.precision(precision);
    ss << d;
    return ss.str();
}



// Accumulate results here, so that they can't be compiled away:
double garbage = 0.0;
// Runs code for at least the given time; returns the number of times run, and the total runtime.
std::pair<long, double> bench(std::function<double()> f, double at_least = 1.0) {
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
    constexpr double mu = approx_zero, sigma = approx_one;

    auto &rng = eris::random::rng();

    // Busy loop to get CPU speed up
    bench([&]() -> double { return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, -1.0, 1.0); },
            2.0);

    // Uniform: draw from a very small range of values so that the probability of acceptance is
    // very close to 1.  Do this at a few different left values just to see that we are getting a
    // consistent result.
    for (double left : {-1.0, -0.1, 0.3, 2.5, 10.0}) {
        double right = left + 1e-8;

        const double inv2s2 = 0.5 / (sigma*sigma);
        const double shift2 = left >= mu ? (left - mu)*(left - mu) : right <= mu ? (right - mu)*(right - mu) : 0;
        auto time = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_uniform(rng, mu, left, right, inv2s2, shift2);
                });
        std::cout << "UR[" << left << ",+1e-10]: " << time.second / time.first * 1e9 << "ns\n";
    }

    for (double left : {0.0}) {
        double right = std::numeric_limits<double>::infinity();
        auto time = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, left, right, left >= mu);
                    });
        std::cout << "HR: " << time.second / time.first * 1e9 << "ns\n";
    }

    for (double left : {2.0, 10.0, -10.0, 100.0, -100.0}) {
        double right = std::numeric_limits<double>::infinity();
        double bound_dist = left >= mu ? (left - mu) : (mu - right);
        auto time = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, left, right, left >= mu, bound_dist, bound_dist);
        });
        std::cout << "ER(" << left << "): " << time.second / time.first * 1e9 << "ns\n";
    }

    for (double left : {-std::numeric_limits<double>::infinity()}) {
        double right = std::numeric_limits<double>::infinity();
        auto time = bench([&]() -> double {
                return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, left, right);
        });
        std::cout << "NR: " << time.second / time.first * 1e9 << "ns\n";
    }


    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";

}
