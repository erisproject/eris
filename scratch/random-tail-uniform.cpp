#include <eris/random/rng.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/random/uniform_real_distribution.hpp>
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
    auto &rng = eris::random::rng();
    // In millionths:
    const int micro_left = 5000000, start = 5050000, end = 5100000;
    double left = micro_left / 1000000.0;
    double at_least = 0.25;
    constexpr int incr = 1000;
    for (int i = start; i <= end; i += incr) {
        double r = i / 1000000.0;
        auto bunif = bench([&]() -> double {
            double x, rho;
            long debug_count = 0;
            do {
                debug_count++;
                x = boost::random::uniform_real_distribution<double>(left, r)(rng);
                rho = exp(-left * (x - left));
            } while (rho < boost::random::uniform_01<double>()(rng));
            return x;
        }, at_least);
        auto bexp = bench([&]() -> double {
            double x;
            do {
                x = left + eris::random::exponential_distribution<double>(left)(rng);
            } while (x >= r);
            return x;
        }, at_least);
        std::cout << "["<<left<<","<<r<<"]: UR: " << bunif.first / bunif.second / 1000000 << " Mdraws/s; ER: " <<
            bexp.first / bexp.second / 1000000 << " Mdraws/s" << std::endl;
    }

    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";
}
