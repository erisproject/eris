#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/lexical_cast.hpp>
#include <random>
#include <iomanip>
#include <chrono>
#include <string>
#include <unordered_map>
extern "C" {
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_cdf.h>
}

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

template <typename T>
struct calls_result {
    unsigned long calls;
    double seconds;
    T mean;
};

boost::random::mt19937 rng_boost;
std::mt19937 rng_stl;
gsl_rng *rng_gsl;

// library => { testname => cost }; if library is "", this is op costs
std::unordered_map<std::string, std::unordered_map<std::string, double>> cost;
// The libraries (in the order we want to display them)
std::vector<std::string> rng_libs = {"stl", "boost", "gsl"};
// c_op is cleaner to type than cost[""]:
std::unordered_map<std::string, double> &c_op = cost[""];

constexpr unsigned incr = 1<<21; // ~ 2 million, but also a power of 2, which may *slightly* improve the mean
                                 // calculation in callTest, particular in the default one-iteration case
double last_benchmark_ns = std::numeric_limits<double>::quiet_NaN(), benchmark_overhead = last_benchmark_ns, benchmark_overhead_f = benchmark_overhead;

boost::math::normal_distribution<double> N01d;

// Call a given function (or function-like object) 2 million times, repeating until at least the given number of
// seconds has elapsed.  Returns a calls_result with the number of draws, total elapsed time, and
// mean of the draws.
template <typename Callable> auto callTest(const Callable &callable, double seconds) -> calls_result<decltype(callable())> {
    calls_result<decltype(callable())> ret = {0, 0, 0};
    auto start = clk::now();
    // Volatile so that it shouldn't be optimized away
    volatile decltype(callable()) value_store;
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

// Default number of seconds to benchmark; 0 means 1 iteration (of 2 million calls)
double bench_seconds = 0.0;
// Benchmark a function by calling it for at least 2 seconds and printing the speed
template <typename Callable> auto benchmark(const std::string &name, const Callable &c) -> decltype(c()) {
    auto result = callTest(c, bench_seconds);
    last_benchmark_ns = 1000000000*result.seconds / result.calls;
    std::cout << std::setw(30) << std::left << name + ":" << std::right << std::fixed << std::setprecision(2) <<
        std::setw(7) << 1000/last_benchmark_ns << " MHz = " << std::setw(8) << last_benchmark_ns << " ns/op";
    double &overhead = (std::is_same<decltype(c()), float>::value ? benchmark_overhead_f : benchmark_overhead);
    if (not std::isnan(overhead)) {
        std::cout << "; net of overhead: " << std::setw(8) << last_benchmark_ns - overhead << " ns/op";
    }
    std::cout << "\n";
    std::cout.unsetf(std::ios_base::floatfield);
    return result.mean;
}
// Benchmark a RNG distribution by constructing it then calling it a bunch of times
template <typename Dist, typename RNG, typename... Args> typename Dist::result_type benchmarkDraw(const std::string &name, RNG &rng, Args&&... args) {
    Dist dist(std::forward<Args>(args)...);
    return benchmark(name, [&dist,&rng]() -> typename Dist::result_type { return dist(rng); });
}

// LambertW approximation (using Newton's method)
double lambertW(const double z, const double tol = 1e-12) {
    if (z == 0) return 0;
    else if (z < 0) throw "not handled";

    double wcur, wnext = 1;
    do {
        wcur = wnext;
        double ew = std::exp(wcur);
        wnext = wcur - (wcur*ew - z) / (ew + wcur*ew);
    }
    while (std::fabs((wnext - wcur) / wcur) > tol);

    return wnext;
}

// Calculates a root by starting at right/left and cutting off half the space each time until right
// and left are within the given tolerance.  f(left) and f(right) must have opposite signs; if the
// function has multiple roots, this will find an arbitrary one.  If the function is not continuous
// with a discontinuity that changes sign, this may well find the discontinuity.
double root(const std::function<double(double)> &f, double left, double right, double tol) {
    bool increasing = std::signbit(f(left));
    bool right_incr = !std::signbit(f(right));
    if (increasing != right_incr) throw std::logic_error(u8"Unable to calculate root: f(left) and f(right) have same sign");

    while (right - left > tol*left) {
        double mid = 0.5*(right + left);
        double fmid = f(mid);
        if (fmid > 0) (increasing ? right : left) = mid;
        else if (fmid < 0) (increasing ? left : right) = mid;
        else return mid; // Found exactly 0
    }
    return 0.5*(right+left);
}


// This returns the value of a at which the benefit of using 1/a as an approximation in the decision
// between ER and UR sampling equals the expected value of the cost increase due to using the
// suboptimal UR when ER would be better.  The approximation is:
//  1/a =~ 2/(a+sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)
// In short, when a is above this value, use the approximation to determine the threshold b value
// above which ER is preferred.
//
// The approximation is always larger than the true value, and so errors here involve using UR when
// ER would be better; the returned value is the point at which the extra cost of the full
// calculation equals the expected extra cost of using the inferior choice.
//
// Parameters:
//
// library - the cost library (e.g. "boost") from which to read RNG cost values.  In particular,
// the following cost values must be set in cost[library]:
//     - "ER" - the cost of an exponential rejection draw (including related acceptance draws and calculations)
//     - "UR" - the cost of an uniform rejection draw (including related acceptance draws and calculations)
// The following must also be set in cost[""]:
//     - "sqrt" or "sqrt(f)" - the cost of a sqrt; the latter is used if float_op is true.
//     - "e^x" or "e^x(f)" - the cost of calculating an exponential; the latter is used if float_op
//     is true.
//
// float_op - if true, use the float costs of sqrt and e^x instead of the double costs to calculate
// the cost savings.
//
// tol - the relative tolerance desired for the returned value.
//
double a1(const std::string &library, bool float_op = false, const double tol=1e-10) {
    const double cer_over_cur = cost[library].at("ER")/cost[library].at("UR"),
          ccheck_over_cur = (c_op.at(float_op ? "sqrt(f)" : "sqrt") + c_op.at(float_op ? "e^x(f)" : "e^x")) / cost[library].at("UR");
    return root([&cer_over_cur, &ccheck_over_cur](const double a) -> double {
        const double sqrta2p4 = std::sqrt(a*a + 4);
        return
            (
                cer_over_cur * std::exp(-0.5*a*a)
                /
                (boost::math::constants::root_two_pi<double>() * (cdf(complement(N01d, a)) - cdf(complement(N01d, a + cer_over_cur/a))))
            )
            *
            (
                1/a - 2 / (a + sqrta2p4) * std::exp(0.5 + 0.25*(a*a - a*sqrta2p4))
            )
            -
            ccheck_over_cur;
    }, 1e-10, 10.1, tol);
}

// Returns the expected extra cost incurred by using the given approximation of e^(a^2/2) instead of
// the actual calculation when deciding between UR and HR.
inline double exp_cost_delta(const double a, const double approx_exp_halfaa, const double chr, const double cur) {
    return 0.5 * chr
        / (cdf(complement(N01d, a)) - cdf(complement(N01d, a + chr/cur * boost::math::constants::root_half_pi<double>() * approx_exp_halfaa)))
        * (1 - approx_exp_halfaa / std::exp(0.5*a*a));
}

// This returns the value of a at which the savings of using a Tn approximation of e^x is equal to the
// maximum extra cost incurred by the approximation error, when deciding between uniform rejection
// and half-normal rejection.
//
// Parameters:
//
// n the order of the Taylor approximation for approximating e^x
//
// library - the cost library (e.g. "boost") from which to read RNG cost values.  In particular,
// the following cost values must be set in cost[library]:
//     - "HR" - the cost of half-normal rejection sampling.  Since a half-normal pdf divided by a
//       normal pdf is a constant, half-normal rejection needs no separate rejection draw or
//       calculation, and so this is just the cost of drawing a normal.
//     - "UR" - the cost of a uniform rejection draw (including calculation/draw for acceptance)
// The following must also be set in cost[""]:
//     - "e^x" or "e^x(f)" - the cost of calculating an exponential; the latter is used if float_op
//       is true.
//     - "expT{n}", where {n} is replaced with the value of n passed into the function; this is the
//       cost of the nth order Taylor approximation.
//
// float_op - if true, use speed of floating point exp rather than double exp.
//
// tol - the desired tolerance level.
double aT(const unsigned n, const std::string &library, bool float_op = false, const double tol=1e-10) {
    const double &chr = cost[library].at("HR"), &cur = cost[library].at("UR"),
          cdiff = c_op.at(float_op ? "e^x(f)" : "e^x") - c_op.at("expT" + std::to_string(n));
    return root([&chr, &cur, &cdiff, &n](const double a) -> double {
            const double x = 0.5*a*a;
            double xn = 1;
            long fact = 1;
            double approx = 1;
            for (unsigned i = 1; i <= n; i++) {
                xn *= x;
                fact *= i;
                approx += xn / fact;
            }
            return exp_cost_delta(a, approx, chr, cur) - cdiff;
    }, 1e-10, 10.2, tol);
}

// This returns the value of a above which using the Tn approximation is preferred to using the
// T(n-1) approximation of e^x
//
// Parameters:
//
// n the order of the Taylor approximations for approximating e^x (Tn and T{n-1} are compared).
//
// library - the cost library (e.g. "boost") from which to read RNG cost values.  In particular,
// the following cost values must be set in cost[library]:
//     - "HR" - the cost of half-normal rejection sampling.  Since a half-normal pdf divided by a
//       normal pdf is a constant, half-normal rejection needs no separate rejection draw or
//       calculation, and so this is just the cost of drawing a normal.
//     - "UR" - the cost of a uniform rejection draw (including calculation/draw for acceptance)
// The following must also be set in cost[""]:
//     - "expT{n}" and "expT{n-1}", where {n} and {n-1} are replaced using the value of n passed
//       into the function; these are the costs of the nth and (n-1)th order Taylor approximations.
//
// tol the desired tolerance level.
double aTT1(const unsigned n, const std::string &library, const double tol=1e-10) {
    if (n < 2) throw std::logic_error("aTT1(n, ...) requires n >= 2");

    const double &chr = cost[library].at("HR"), &cur = cost[library].at("UR"),
          cdiff = (c_op.at("expT" + std::to_string(n)) - c_op.at("expT" + std::to_string(n-1)));

    return root([&chr, &cur, &cdiff, &n](const double a) -> double {
            const double x = 0.5*a*a;
            double xn = 1;
            long fact = 1;
            double approx = 1, approx_2ndlast = 1;
            for (unsigned i = 1; i <= n; i++) {
                approx_2ndlast = approx;
                xn *= x;
                fact *= i;
                approx += xn / fact;
            }

            return
                (exp_cost_delta(a, approx_2ndlast, chr, cur) - exp_cost_delta(a, approx, chr, cur)) - cdiff;
    }, 0.01, 3.0, tol);
}

// Test the draw speed of various distributions
int main(int argc, char *argv[]) {
    // If we're given one argument, it's the number of seconds; if 2, it's seconds and a seed
    unsigned long seed = 0;
    if (argc < 1 or argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [SECONDS [SEED]]\n";
        exit(1);
    }
    if (argc >= 2) {
        try {
            bench_seconds = boost::lexical_cast<double>(argv[1]);
        }
        catch (const boost::bad_lexical_cast &) {
            std::cerr << "Invalid SECONDS value `" << argv[1] << "'\n\nUsage: " << argv[0] << " [SECONDS [SEED]]\n";
        }
    }
    if (argc >= 3) {
        try {
            seed = boost::lexical_cast<decltype(seed)>(argv[2]);
        }
        catch (const boost::bad_lexical_cast &) {
            std::cerr << "Invalid SEED value `" << argv[2] << "'\n\nUsage: " << argv[0] << " [SECONDS [SEED]]\n";
            exit(1);
        }
    }
    else {
        std::random_device rd;
        static_assert(sizeof(decltype(rd())) == 4, "Internal error: std::random_device doesn't give 32-bit values!?");
        seed = (uint64_t(rd()) << 32) + rd();
    }
    std::cout << "Using mt19937 generator with seed = " << seed << "\n";
    std::cout << std::showpoint;
    rng_stl.seed(seed);
    rng_boost.seed(seed);
    rng_gsl = gsl_rng_alloc(gsl_rng_mt19937);
    gsl_rng_set(rng_gsl, seed);

    double mean = 0;
    auto default_prec = std::cout.precision();
#define PRECISE(v) std::setprecision(std::numeric_limits<double>::max_digits10) << v << std::setprecision(default_prec)

    // Some constants to use below.  These are declared volatile to prevent the compiler from
    // optimizing them away.
    volatile const double ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, onehalf = 0.5, eight = 8.,
             e = boost::math::constants::e<double>(), pi = boost::math::constants::pi<double>(),
             piandahalf = 1.5*pi, pointone = 0.1, one = 1.0, onepointfive = 1.5;

    volatile const float eightf = 8.f, minustwof = -2.f, tenf = 10.f, minustenf = -10.f, twof = 2.f, onehalff = 0.5f,
             piandahalff = piandahalf, ef = boost::math::constants::e<float>();

    // Modern CPUs have a variable clock, and may take a few ms to increase to maximum frequency, so
    // run a fake test for a second to (hopefully) get the CPU at full speed.
    callTest([]() -> double { return 1.0; }, 1.0);

    // NB: square brackets around values below indicate compiler time constants (or, at least,
    // constexprs, which should work the same if the compiler is optimizing)
    mean += benchmark("overhead (d)", [&]() -> double { return eight; });
    benchmark_overhead = last_benchmark_ns;
    mean += benchmark("overhead (f)", [&]() -> float { return eightf; });
    benchmark_overhead_f = last_benchmark_ns;

    mean += benchmark("evaluate (d) exp(10)", [&]() -> double { return std::exp(ten); });
    c_op["e^x"] += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(-10)", [&]() -> double { return std::exp(minusten); });
    c_op["e^x"] += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(2)", [&]() -> double { return std::exp(two); });
    c_op["e^x"] += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(-2)", [&]() -> double { return std::exp(minustwo); });
    c_op["e^x"] += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(1.5pi)", [&]() -> double { return std::exp(piandahalf); });
    c_op["e^x"] += last_benchmark_ns;
    c_op["e^x"] /= 5;
    c_op["e^x"] -= benchmark_overhead;
    mean += benchmark("evaluate (f) exp(10)", [&]() -> float { return std::exp(tenf); });
    c_op["e^x(f)"] += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(-10)", [&]() -> float { return std::exp(minustenf); });
    c_op["e^x(f)"] += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(2)", [&]() -> float { return std::exp(twof); });
    c_op["e^x(f)"] += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(-2)", [&]() -> float { return std::exp(minustwof); });
    c_op["e^x(f)"] += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(1.5pi)", [&]() -> float { return std::exp(piandahalff); });
    c_op["e^x(f)"] += last_benchmark_ns;
    c_op["e^x(f)"] /= 5;
    c_op["e^x(f)"] -= benchmark_overhead_f;
    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "Testing exp approximations\n" << std::setprecision(8);
    mean = 0;
#define DUMP_MEAN std::cout << "    -> " << PRECISE(mean) << "\n"; mean = 0

    // Calculate the costs of Taylor expansions of order n; 1 is 0 (because 1 is the value 1+x, which is basically free)
    c_op["expT1"] = 0;
    // The "expT_n" values are nth order Taylor expansions of e^x
    mean += benchmark("evaluate (d) expT_2(0.5)", [&]() -> double { double x = onehalf; return 1. + x + 0.5*x*x; });
    c_op["expT2"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_3(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1 + 0.5*x*(1 + (1./3.)*x)); });
    c_op["expT3"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_4(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    c_op["expT4"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_5(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x)))); });
    c_op["expT5"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_6(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x*(1. + 1./6.*x))))); });
    c_op["expT6"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_7(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x*(1. + 1./6.*x*(1. + 1./7.*x)))))); });
    c_op["expT7"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) expT_8(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x*(1. + 1./6.*x*(1. + 1./7.*x*(1. + 1./8.*x))))))); });
    c_op["expT8"] = last_benchmark_ns - benchmark_overhead;

    mean += benchmark("evaluate (d) expT_12(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x*(1. + 1./6.*x*(1. + 1./7.*x*(1. + 1./8.*x*
                                        (1.+1./9.*x*(1.+1./10.*x*(1.+1./11.*x*(1.+1./12.*x))))
                                         ))))))); });
    DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_4(0.01)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    DUMP_MEAN;

    mean += benchmark("evaluate (f) expT_2(0.5)", [&]() -> float { float x = onehalff; return 1.f + x + 0.5f*x*x; });
    DUMP_MEAN;
    mean += benchmark("evaluate (f) expT_4(0.5)", [&]() -> float { float x = onehalff; return 1.f + x*(1.f + 1.f/2.f*x*(1.f + 1.f/3.f*x*(1.f + 1.f/4.f*x))); });
    DUMP_MEAN;
    mean += benchmark("evaluate (f) expT_8(0.5)", [&]() -> float { float x = onehalff; return 1.f + x*(1.f + 1.f/2.f*x*(1.f + 1.f/3.f*x*(1.f + 1.f/4.f*x*(1.f + 1.f/5.f*x*(1.f + 1.f/6.f*x*(1.f + 1.f/7.f*x*(1.f + 1.f/8.f*x))))))); });
    DUMP_MEAN;

    mean += benchmark("evaluate (d) expT_4(0.1)", [&]() -> double { double x = pointone; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(0.1)=" << PRECISE(std::exp(0.1)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_4(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(0.5)=" << PRECISE(std::exp(0.5)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_4(1.0)", [&]() -> double { double x = one; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(1.0)=" << PRECISE(std::exp(1.0)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_4(1.5)", [&]() -> double { double x = onepointfive; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(1.5)=" << PRECISE(std::exp(1.5)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_4(2.0)", [&]() -> double { double x = two; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(2.0)=" << PRECISE(std::exp(2.0)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) expT_5(2.0)", [&]() -> double { double x = two; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x)))); });
    std::cout << "exp(2.0)=" << PRECISE(std::exp(2.0)) << "; approximation: "; DUMP_MEAN;

    std::cout.precision(default_prec);

    mean += benchmark("evaluate (d) log(10)", [&]() -> double { return std::log(ten); });
    mean += benchmark("evaluate (d) log(piandahalf)", [&]() -> double { return std::log(piandahalf); });
    mean += benchmark("evaluate (d) log(e)", [&]() -> double { return std::log(e); });
    mean += benchmark("evaluate (f) log(10)", [&]() -> float { return std::log(tenf); });
    mean += benchmark("evaluate (f) log(piandahalf)", [&]() -> float { return std::log(piandahalff); });
    mean += benchmark("evaluate (f) log(e)", [&]() -> float { return std::log(ef); });

    mean += benchmark("evaluate (d) sqrt(8)", [&]() -> double { return std::sqrt(eight); });
    mean += benchmark("evaluate (d) sqrt(1.5pi)", [&]() -> double { return std::sqrt(piandahalf); });
    c_op["sqrt"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (f) sqrt(8)", [&]() -> float { return std::sqrt(eightf); });
    mean += benchmark("evaluate (f) sqrt(1.5pi)", [&]() -> float { return std::sqrt(piandahalff); });
    c_op["sqrt(f)"] = last_benchmark_ns - benchmark_overhead_f;

    mean += benchmark("evaluate [1]/pi", [&]() -> double { return 1.0/pi; });
    mean += benchmark("evaluate [1]/sqrt(pi)", [&]() -> double { return 1.0/std::sqrt(pi); });
    mean += benchmark("evaluate sqrt([1]/pi)", [&]() -> double { return std::sqrt(1.0/pi); });

    mean += benchmark("evaluate [1]/pi", [&]() -> double { return 1.0/pi; });
    mean += benchmark("evaluate [1]/sqrt(pi)", [&]() -> double { return 1.0/std::sqrt(pi); });
    mean += benchmark("evaluate sqrt([1]/pi)", [&]() -> double { return std::sqrt(1.0/pi); });

    mean += benchmark("evaluate e*pi", [&]() -> double { return e*pi; });
    mean += benchmark("evaluate e+pi", [&]() -> double { return e+pi; });
    mean += benchmark("evaluate e*([2]+pi)", [&]() -> double { return e*(2.0+pi); });
    mean += benchmark("evaluate e*[0.5]*([2]+pi)", [&]() -> double { return e*0.5*(2.0+pi); });
    mean += benchmark("evaluate e*e*...*e (e^10)", [&]() -> double { return e*e*e*e*e*e*e*e*e*e; });
    mean += benchmark("evaluate pi*pi", [&]() -> double { return pi*pi; });
    // The compiler should be smart enough to de-pow this one:
    mean += benchmark("evaluate pow(pi,[2])", [&]() -> double { return std::pow(pi, 2.0); });
    // Since `two` is volatile, it can't here; performance will depend on how well the c++ library
    // can handle integer powers.
    mean += benchmark("evaluate pow(pi,2)", [&]() -> double { return std::pow(pi, two); });
    // This one is typically very slow:
    mean += benchmark("evaluate pow(pi,2.0001)", [&]() -> double { return std::pow(pi, 2.0001); });
    mean += benchmark("evaluate N cdf (boost)", [&]() -> double { return cdf(N01d, two); });
    mean += benchmark("evaluate N pdf (boost)", [&]() -> double { return pdf(N01d, two); });
    mean += benchmark("evaluate N cdf (gsl)", [&]() -> double { return gsl_cdf_ugaussian_P(two); });
    mean += benchmark("evaluate N pdf (gsl)", [&]() -> double { return gsl_ran_ugaussian_pdf(two); });
    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(1e9,2e7)", rng_boost, 1e9, 2e7);
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[1e9,1e10)", rng_boost, 1e9, 1e10);
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(30)", rng_boost, 30);

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(0,1)", rng_boost);
    cost["boost"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[0,1)", rng_boost);
    cost["boost"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(1)", rng_boost);
    cost["boost"]["Exp"] = last_benchmark_ns - benchmark_overhead;

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(1e9,2e7)", rng_stl, 1e9, 2e7);
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[1e9,1e10)", rng_stl, 1e9, 1e10);
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(30)", rng_stl, 30);

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(0,1)", rng_stl);
    cost["stl"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[0,1)", rng_stl);
    cost["stl"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(1)", rng_stl);
    cost["stl"]["Exp"] = last_benchmark_ns - benchmark_overhead;

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("gsl N(1e9,2e7) (Box-Mul.)", [&]() -> double { return 1e9 + gsl_ran_gaussian(rng_gsl, 2e7); });
    mean += benchmark("gsl N(1e9,2e7) (ratio)", [&]() -> double { return 1e9 + gsl_ran_gaussian_ratio_method(rng_gsl, 2e7); });
    mean += benchmark("gsl N(1e9,2e7) (ziggurat)", [&]() -> double { return 1e9 + gsl_ran_gaussian_ziggurat(rng_gsl, 2e7); });
    mean += benchmark("gsl U[1e9,1e10]", [&]() -> double { return gsl_ran_flat(rng_gsl, 1e9, 1e10); });
    mean += benchmark("gsl Exp(1/30)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1./30.); });

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    mean += benchmark("gsl N(0,1) (Box-Muller)", [&]() -> double { return gsl_ran_gaussian(rng_gsl, 1); });
    mean += benchmark("gsl N(0,1) (ratio)", [&]() -> double { return gsl_ran_gaussian_ratio_method(rng_gsl, 1); });
    mean += benchmark("gsl N(0,1) (ziggurat)", [&]() -> double { return gsl_ran_gaussian_ziggurat(rng_gsl, 1); });
    cost["gsl"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl U[0,1]", [&]() -> double { return gsl_ran_flat(rng_gsl, 0, 1); });
    cost["gsl"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl Exp(1)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1); });
    cost["gsl"]["Exp"] = last_benchmark_ns - benchmark_overhead;

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    for (const auto &l : rng_libs) {
        cost[l]["HR"] = cost[l]["NR"] = cost[l].at("N");
        cost[l]["ER"] = cost[l].at("Exp") + cost[l].at("U") + c_op.at("e^x");
        cost[l]["UR"] = 2*cost[l].at("U") + c_op.at("e^x");
    }

    for (const auto &l : rng_libs) {
        double &sqrtL = cost[l]["sqrtL"];
        sqrtL = std::sqrt(lambertW(2/M_PI*M_E*M_E*std::pow(cost[l]["ER"]/cost[l]["HR"], 2)));
        cost[l]["a0"] = sqrtL - 1.0/sqrtL;
        cost[l]["a1"] = a1(l);
        cost[l]["a1(f)"] = a1(l, true);
    }

    std::cout << "\n\n\nSummary:\n\n" <<
        std::fixed << std::setprecision(4) <<

        "\nOperations:\n\n" <<
      u8"    c_√ (double)         = " << std::setw(8) << c_op["sqrt"] << "\n" <<
      u8"    c_e^x (double)       = " << std::setw(8) << c_op["e^x"]    << "\n" <<
      u8"    c_√ + c_e^x (double) = " << std::setw(8) << c_op["sqrt"] + c_op["e^x"] << "\n" <<
      "\n" <<
      u8"    c_√ (float)          = " << std::setw(8) << c_op["sqrt(f)"] << "\n" <<
      u8"    c_e^x (float)        = " << std::setw(8) << c_op["e^x(f)"]    << "\n" <<
      u8"    c_√ + c_e^x (float)  = " << std::setw(8) << c_op["sqrt(f)"] + c_op["e^x(f)"] << "\n" <<
        "\n\n";

    constexpr int fieldwidth = 35;
    std::cout << "Draws:" << std::setw(fieldwidth-2) << "";
#define FOR_l for (const auto &l : rng_libs)
    FOR_l { std::cout << std::setw(8) << std::right << l << "    ";  }
    std::cout << "\n" << std::setw(4+fieldwidth) << "";
    for (unsigned i = 0; i < rng_libs.size(); i++) { std::cout << " -------    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_NR = c_HR = c_n" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("NR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_ER = c_exp + c_e^x + c_u" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("ER") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_UR = 2 c_u + c_e^x" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("UR") << "    "; }

    std::cout << u8"\n\n    a₀" << std::setw(fieldwidth-2) << std::left << " | c_ER, c_HR" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a0") << "    "; }

    // NB: the string.length()-2 thing here is to fool setw into properly displaying the literal
    // utf8 label
    std::cout << u8"\n\n    " << std::setw(fieldwidth+std::string{u8"₁√"}.length()-2) << std::left << "a₁ | c_ER, c_UR, c_√, c_e^x" << std::right;
    bool show_dagger = false;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1"); if (cost[l].at("a1") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is trivially satisfied";
    show_dagger = false;

    std::cout << u8"\n\n    " << std::setw(fieldwidth+std::string{u8"₁√"}.length()-2) << std::left << "a₁ | c_ER, c_UR, c_√(f), c_e^x(f)" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1(f)"); if (cost[l].at("a1(f)") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is trivially satisfied";
    show_dagger = false;

    gsl_rng_free(rng_gsl);
    std::cout << "\n\n\n";
}
