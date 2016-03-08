#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
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
    std::cout << std::setw(25) << std::left << name + ":" <<
        std::setw(8) << speed << " MHz = " << std::setw(8) << 1000/speed << " ns/op\n";
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

    double mean = 0;
    auto default_prec = std::cout.precision();
#define PRECISE(v) std::setprecision(std::numeric_limits<double>::max_digits10) << v << std::setprecision(default_prec)

    // Modern CPUs have a variable clock, and may take a few ms to increase to maximum frequency, so
    // run a fake test for a second to (hopefully) get the CPU at full speed.
    {
        auto fake = [&](decltype(rng)&) -> double { return 1.0; };
        drawTest(fake, 1.0);
    }
    volatile double ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, eight = 8.,
             e = std::exp(1), pi = boost::math::constants::pi<double>();
    mean += benchmark("constant (e)", [&](decltype(rng)&) -> double { return e; });
    mean += benchmark("evaluate exp(10)", [&](decltype(rng)&) -> double { return std::exp(ten); });
    mean += benchmark("evaluate exp(-10)", [&](decltype(rng)&) -> double { return std::exp(minusten); });
    mean += benchmark("evaluate exp(-2)", [&](decltype(rng)&) -> double { return std::exp(minustwo); });
    mean += benchmark("evaluate exp(pi)", [&](decltype(rng)&) -> double { return std::exp(pi); });
    mean += benchmark("evaluate sqrt(8)", [&](decltype(rng)&) -> double { return std::sqrt(eight); });
    mean += benchmark("eval 1/pi", [&](decltype(rng)&) -> double { return 1.0/pi; });
    mean += benchmark("eval 1/sqrt(pi)", [&](decltype(rng)&) -> double { return 1.0/std::sqrt(pi); });
    mean += benchmark("eval sqrt(1/pi)", [&](decltype(rng)&) -> double { return std::sqrt(1.0/pi); });
    mean += benchmark("evaluate e*pi", [&](decltype(rng)&) -> double { return e*pi; });
    mean += benchmark("evaluate e+pi", [&](decltype(rng)&) -> double { return e+pi; });
    mean += benchmark("evaluate e*(2+pi)", [&](decltype(rng)&) -> double { return e*(2+pi); });
    mean += benchmark("evaluate e*0.5*(2+pi)", [&](decltype(rng)&) -> double { return e*0.5*(2+pi); });
    mean += benchmark("e*e*...*e (e^10)", [&](decltype(rng)&) -> double { return e*e*e*e*e*e*e*e*e*e; });
    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("boost N(1e9,2e7)", boost::random::normal_distribution<double>(1e9, 2e7));
    mean += benchmark("boost U[1e9,1e10)", boost::random::uniform_real_distribution<double>(1e9, 1e10));
    mean += benchmark("boost Exp(30)", boost::random::exponential_distribution<double>(30));

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("boost N(0,1)", boost::random::normal_distribution<double>());
    mean += benchmark("boost U[0,1)", boost::random::uniform_real_distribution<double>());
    mean += benchmark("boost Exp(1)", boost::random::exponential_distribution<double>());

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    boost::math::normal_distribution<double> N01;
    mean += benchmark("N cdf", [&](decltype(rng)&) { return cdf(N01, two); });
    mean += benchmark("N pdf", [&](decltype(rng)&) { return pdf(N01, two); });

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("stl N(1e9,2e7)", std::normal_distribution<double>(1e9, 2e7));
    mean += benchmark("stl U[1e9,1e10)", std::uniform_real_distribution<double>(1e9, 1e10));
    mean += benchmark("stl Exp(30)", std::exponential_distribution<double>(30));

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("stl N(0,1)", std::normal_distribution<double>(0, 1));
    mean += benchmark("stl U[0,1)", std::uniform_real_distribution<double>(0, 1));
    mean += benchmark("stl Exp(1)", std::exponential_distribution<double>(1));

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}
