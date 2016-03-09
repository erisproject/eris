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

struct calls_result {
    unsigned long calls;
    double seconds, mean;
};

boost::random::mt19937_64 rng;
constexpr unsigned incr = 2000000;
// Variable in which to store call return values.  Declared volatile so that it can't be optimized
// away.
volatile double value_store;
double last_benchmark_ns = std::numeric_limits<double>::quiet_NaN(), benchmark_overhead = last_benchmark_ns;

// Call a given function (or function-like object) 2 million times, repeating until at least the given number of
// seconds has elapsed.  Returns a calls_result with the number of draws, total elapsed time, and
// mean of the draws.
template <typename Callable> calls_result callTest(const Callable &callable, double seconds) {
    calls_result ret = {};
    auto start = clk::now();
    do {
        for (unsigned i = 0; i < incr; i++) {
            value_store = callable();
            ret.mean += value_store;
        }
        ret.calls += incr;
        ret.seconds = dur(clk::now() - start).count();
    } while (ret.seconds < seconds);
    ret.mean /= ret.calls;
    return ret;
}


// Benchmark a function by calling it for at least 3 seconds and printing the speed
template <typename Callable> double benchmark(const std::string &name, const Callable &c) {
    auto result = callTest(c, 3.0);
    last_benchmark_ns = 1000000000*result.seconds / result.calls;
    std::cout << std::setw(25) << std::left << name + ":" << std::right << std::fixed << std::setprecision(2) <<
        std::setw(7) << 1000/last_benchmark_ns << " MHz = " << std::setw(8) << last_benchmark_ns << " ns/op";
    if (not std::isnan(benchmark_overhead)) {
        std::cout << "; net of overhead: " << std::setw(8) << last_benchmark_ns - benchmark_overhead << " ns/op";
    }
    std::cout << "\n" << std::defaultfloat;
    return result.mean;
}
// Benchmark a RNG distribution by constructing it then calling it a bunch of times
template <typename Dist, typename... Args> double benchmarkDraw(const std::string &name, Args&&... args) {
    Dist dist(std::forward<Args>(args)...);
    return benchmark(name, [&dist]() -> double { return dist(rng); });
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
    std::cout << std::showpoint;
    rng.seed(seed);

    double mean = 0;
    auto default_prec = std::cout.precision();
#define PRECISE(v) std::setprecision(std::numeric_limits<double>::max_digits10) << v << std::setprecision(default_prec)

    // Some constants to use below.  These are declared volatile to prevent the compiler from
    // optimizing them away.
    volatile const double ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, eight = 8.,
             e = std::exp(1), pi = boost::math::constants::pi<double>();
    boost::math::normal_distribution<double> N01;

    // Modern CPUs have a variable clock, and may take a few ms to increase to maximum frequency, so
    // run a fake test for a second to (hopefully) get the CPU at full speed.
    callTest([]() -> double { return 1.0; }, 1.0);

    mean += benchmark("overhead", [&]() -> double { return 1.0; });
    benchmark_overhead = last_benchmark_ns;
    mean += benchmark("evaluate exp(10)", [&]() -> double { return std::exp(ten); });
    mean += benchmark("evaluate exp(-10)", [&]() -> double { return std::exp(minusten); });
    mean += benchmark("evaluate exp(-2)", [&]() -> double { return std::exp(minustwo); });
    mean += benchmark("evaluate exp(pi)", [&]() -> double { return std::exp(pi); });
    mean += benchmark("evaluate sqrt(8)", [&]() -> double { return std::sqrt(eight); });
    mean += benchmark("eval 1/pi", [&]() -> double { return 1.0/pi; });
    mean += benchmark("eval 1/sqrt(pi)", [&]() -> double { return 1.0/std::sqrt(pi); });
    mean += benchmark("eval sqrt(1/pi)", [&]() -> double { return std::sqrt(1.0/pi); });
    mean += benchmark("evaluate e*pi", [&]() -> double { return e*pi; });
    mean += benchmark("evaluate e+pi", [&]() -> double { return e+pi; });
    mean += benchmark("evaluate e*(2+pi)", [&]() -> double { return e*(2+pi); });
    mean += benchmark("evaluate e*0.5*(2+pi)", [&]() -> double { return e*0.5*(2+pi); });
    mean += benchmark("e*e*...*e (e^10)", [&]() -> double { return e*e*e*e*e*e*e*e*e*e; });
    mean += benchmark("N cdf", [&]() -> double { return cdf(N01, two); });
    mean += benchmark("N pdf", [&]() -> double { return pdf(N01, two); });
    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(1e9,2e7)", 1e9, 2e7);
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[1e9,1e10)", 1e9, 1e10);
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(30)", 30);

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(0,1)");
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[0,1)");
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(1)");

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(1e9,2e7)", 1e9, 2e7);
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[1e9,1e10)", 1e9, 1e10);
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(30)", 30);

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(0,1)", 0, 1);
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[0,1)", 0, 1);
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(1)", 1);

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}
