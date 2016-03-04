#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <random>
#include <iomanip>
#include <chrono>
#include <string>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

struct draws_result {
    unsigned long draws;
    double seconds, mean;
};

boost::random::mt19937_64 rng;
constexpr unsigned incr = 500000;
// Draw from the given distribution 500,000 times, repeating until at least the given number of
// seconds has elapsed.  Returns a draws_result with the number of draws, total elapsed time, and
// mean of the draws.
template <typename T> draws_result drawTest(T& dist, double seconds) {
    draws_result ret = {};
    auto start = clk::now();
    do {
        for (unsigned i = 0; i < incr; i++) {
            ret.mean += dist(rng);
        }
        ret.draws += incr;
        ret.seconds = dur(clk::now() - start).count();
    } while (ret.seconds < seconds);
    ret.mean /= ret.draws;
    return ret;
}

// Benchmark a generator by drawing from it for at least 3 seconds and printing the speed
template <typename Generator> double benchmark(const std::string &name, Generator &gen) {
    auto result = drawTest(gen, 3.0);
    double speed = result.draws/(1000000*result.seconds);
    std::cout << std::setw(20) << std::left << name + ":" <<
        std::setw(10) << speed << " Mdraws/s;    " << std::setw(10) << 1/speed << " µs/draw\n";
    return result.mean;
}
// rvalue wrapper around the above
template <typename Generator> double benchmark(const std::string &name, Generator &&gen) {
    return benchmark(name, gen);
}

// Test the draw speed of various distributions
int main(int argc, char *argv[]) {
    // If we're given an argument, use that as a seed
    uint64_t seed = 0;
    if (argc == 2) {
        try {
            static_assert(sizeof(unsigned long) == 8 or sizeof(unsigned long long) == 8,
                    "Internal error: don't know how to parse an unsigned 64-bit int on this architecture!");
            seed = sizeof(unsigned long) == 8 ? std::stoul(argv[1]) : std::stoull(argv[1]);
        }
        catch (const std::logic_error &) {
            std::cerr << "Invalid SEED value specified!\n\nUsage: " << argv[0] << " [SEED]\n";
            exit(1);
        }
    }
    else if (argc != 1) {
        std::cerr << "Usage: " << argv[0] << " [SEED]\n";
        exit(1);
    }
    else {
        std::random_device rd;
        static_assert(sizeof(decltype(rd())) == 4, "Internal error: std::random_device doesn't give 32-bit values!?");
        seed = (uint64_t(rd()) << 32) + rd();
    }
    std::cout << "Using mt19937_64 generator with seed = " << seed << "\n";
    rng.seed(seed);

    double meana = 0, meanb = 0, meanc = 0, meand = 0;

    meana += benchmark("boost N(1e9,2e7)", boost::random::normal_distribution<double>(1e9, 2e7));
    meana += benchmark("boost U[1e9,1e10)", boost::random::uniform_real_distribution<double>(1e9, 1e10));
    meana += benchmark("boost Exp(30)", boost::random::exponential_distribution<double>(30));

    std::cout << "sum of these means: " << std::setprecision(17) << meana << "\n";

    std::cout << "\n";
    meanb += benchmark("boost N(0,1)", boost::random::normal_distribution<double>(0, 1));
    meanb += benchmark("boost U[0,1)", boost::random::uniform_real_distribution<double>(0, 1));
    meanb += benchmark("boost Exp(1)", boost::random::exponential_distribution<double>(1));

    std::cout << "sum of these means: " << std::setprecision(17) << meanb << "\n";

    std::cout << "\n";
    meanc += benchmark("stl N(1e9,2e7)", std::normal_distribution<double>(1e9, 2e7));
    meanc += benchmark("stl U[1e9,1e10)", std::uniform_real_distribution<double>(1e9, 1e10));
    meanc += benchmark("stl Exp(30)", std::exponential_distribution<double>(30));

    std::cout << "sum of these means: " << std::setprecision(17) << meanc << "\n";

    std::cout << "\n";
    meand += benchmark("stl N(0,1)", std::normal_distribution<double>(0, 1));
    meand += benchmark("stl U[0,1)", std::uniform_real_distribution<double>(0, 1));
    meand += benchmark("stl Exp(1)", std::exponential_distribution<double>(1));

    std::cout << "sum of these means: " << std::setprecision(17) << meand << "\n";
}
