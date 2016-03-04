#include <eris/random/rng.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <iomanip>
#include <chrono>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

struct draws_result {
    unsigned long draws;
    double seconds, sum;
};

constexpr unsigned incr = 500000;
// Draw from the given distribution 500,000 times, repeating until at least the given number of
// seconds has elapsed.  Returns a draws_result with the number of draws, total elapsed time, and
// sum of the draws.
template <typename T> draws_result drawTest(T& dist, eris::random::rng_t &rng, double seconds = 2.0) {
    draws_result ret = {};
    auto start = clk::now();
    do {
        for (unsigned i = 0; i < incr; i++) {
            double d = dist(rng);
            ret.sum += d;
        }
        ret.draws += incr;
        ret.seconds = dur(clk::now() - start).count();
    } while (ret.seconds < seconds);
    return ret;
}

// Test the draw speed of various distributions
int main() {
    auto &rng = eris::random::rng();

    boost::random::normal_distribution<double> normal(1e9, 2e7);
    boost::random::uniform_real_distribution<double> unif(1e9, 1e10);
    boost::random::exponential_distribution<double> exp(20);

#define RESULTS(NAME, RES) std::setw(10) << std::left << NAME ":" << \
    std::setw(10) << RES.draws/(1000000*RES.seconds) << " Mdraws/s; " << \
    std::setw(10) << (1000000*RES.seconds)/RES.draws << u8" μs/draw"

    auto result_n = drawTest(normal, rng, 5.0);
    std::cout << RESULTS("N(0,1)", result_n) << "\n";
    auto result_u = drawTest(unif, rng, 5.0);
    std::cout << RESULTS("U[0,1]", result_u) << "\n";
    auto result_e = drawTest(exp, rng, 5.0);
    std::cout << RESULTS("Exp(1)", result_e) << "\n";

    // Do this just so the compiler can't optimize away the results
    if (result_n.sum + result_u.sum + result_e.sum == 0.0) {
        std::cout << "Something went wrong: the sum of all the draws equals 0!\n";
    }
}
