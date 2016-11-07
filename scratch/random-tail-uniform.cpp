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
    for (const long &micro_left : {550000, 750000, 1000000, 2000000, 3000000, 4000000, 5000000, 20000000, 100000000}) {
        const long start = micro_left + 210000000000l/micro_left;
        const long end   = micro_left + 400000000000l/micro_left;
        const long incr = 5000000000l/micro_left;
        double left = micro_left / 1000000.0;
        double at_least = 0.25;
        for (long i = start; i <= end; i += incr) {
            double r = i / 1000000.0;
            double shift2 = left*left;
            double mu = 1e-300;
            double sd = 1+1e-15;
            double inv2s2 = 0.5 / (sd*sd);
            auto bunif = bench([&]() -> double {
                double x, rho;
                long debug_count = 0;
                do {
                    debug_count++;
                    x = boost::random::uniform_real_distribution<double>(left, r)(rng);
                    rho = exp(inv2s2 * (shift2 - (x-mu)*(x-mu)));
                } while (rho < boost::random::uniform_01<double>()(rng));
                return x;
            }, at_least);
            double twice_sigma_squared = 2*sd*sd;
            double x_scale = sd*sd / left;
            double x_delta = 0;
            double x_range = r - left;
            auto bexp = bench([&]() -> double {
                double x;
                do {
                    do {
                        x = x_scale * eris::random::exponential_distribution<double>()(rng);
                    } while (x >= x_range);
                } while (twice_sigma_squared * eris::random::exponential_distribution<double>()(rng) <= (x+x_delta)*(x+x_delta));
                return left + x;
            }, at_least);
            std::cout << "["<<left<<",+"<<r-left<<"]: UR: " << bunif.first / bunif.second / 1000000 << " Mdraws/s; ER: " <<
                bexp.first / bexp.second / 1000000 << " Mdraws/s" << std::endl;
        }
    }

    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";
}
