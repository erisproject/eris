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

constexpr unsigned incr = 2000000;
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

    return wnext;//wnext * std::exp(wnext);
}

double astar(const double cer, const double ccheck, const double cur, const double tol=1e-10) {
    const double cer_over_cur = cer/cur, ccheck_over_cur = ccheck/cur;
    double left = 1e-10, right = 10;
    auto f = [&cer_over_cur, &ccheck_over_cur](const double a) -> double {
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
    };
    if (f(right) > 0 or f(left) < 0) throw std::logic_error("Unable to calculate astar (end-points not right)");

    while (right - left > tol*left) {
        double mid = 0.5*(right + left);
        double fmid = f(mid);
        if (fmid > 0) left = mid;
        else if (fmid < 0) right = mid;
        else return mid;
    }
    return 0.5*(right+left);
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
    volatile const double ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, eight = 8.,
             e = boost::math::constants::e<double>(), pi = boost::math::constants::pi<double>(),
             piandahalf = 1.5*pi;

    volatile const float eightf = 8.f, minustwof = -2.f, tenf = 10.f, minustenf = -10.f, piandahalff = piandahalf;

    // Modern CPUs have a variable clock, and may take a few ms to increase to maximum frequency, so
    // run a fake test for a second to (hopefully) get the CPU at full speed.
    callTest([]() -> double { return 1.0; }, 1.0);

    // NB: square brackets around values below indicate compiler time constants (or, at least,
    // constexprs, which should work the same if the compiler is optimizing)
    mean += benchmark("overhead (d)", [&]() -> double { return eight; });
    benchmark_overhead = last_benchmark_ns;
    mean += benchmark("overhead (f)", [&]() -> float { return eightf; });
    benchmark_overhead_f = last_benchmark_ns;

    double c_e = 0;
    mean += benchmark("evaluate (d) exp(10)", [&]() -> double { return std::exp(ten); });
    c_e += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(-10)", [&]() -> double { return std::exp(minusten); });
    c_e += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(-2)", [&]() -> double { return std::exp(minustwo); });
    c_e += last_benchmark_ns;
    mean += benchmark("evaluate (d) exp(1.5pi)", [&]() -> double { return std::exp(piandahalf); });
    c_e += last_benchmark_ns;
    c_e /= 4;
    c_e -= benchmark_overhead;
    double c_e_f = 0;
    mean += benchmark("evaluate (f) exp(10)", [&]() -> float { return std::exp(tenf); });
    c_e_f += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(-10)", [&]() -> float { return std::exp(minustenf); });
    c_e_f += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(-2)", [&]() -> float { return std::exp(minustwof); });
    c_e_f += last_benchmark_ns;
    mean += benchmark("evaluate (f) exp(1.5pi)", [&]() -> float { return std::exp(piandahalff); });
    c_e_f += last_benchmark_ns;
    c_e_f /= 4;
    c_e_f -= benchmark_overhead_f;

    mean += benchmark("evaluate (d) sqrt(8)", [&]() -> double { return std::sqrt(eight); });
    mean += benchmark("evaluate (d) sqrt(1.5pi)", [&]() -> double { return std::sqrt(piandahalf); });
    double c_sqrt = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (f) sqrt(8)", [&]() -> float { return std::sqrt(eightf); });
    mean += benchmark("evaluate (f) sqrt(1.5pi)", [&]() -> float { return std::sqrt(piandahalff); });
    double c_sqrt_f = last_benchmark_ns - benchmark_overhead_f;

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
    double c_n_boost = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[0,1)", rng_boost);
    double c_u_boost = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(1)", rng_boost);
    double c_exp_boost = last_benchmark_ns - benchmark_overhead;

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
    double c_n_stl = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[0,1)", rng_stl);
    double c_u_stl = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(1)", rng_stl);
    double c_exp_stl = last_benchmark_ns - benchmark_overhead;

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
    double c_n_gsl = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl U[0,1]", [&]() -> double { return gsl_ran_flat(rng_gsl, 0, 1); });
    double c_u_gsl = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl Exp(1)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1); });
    double c_exp_gsl = last_benchmark_ns - benchmark_overhead;

    std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    double c_er_boost = c_exp_boost + c_e + c_u_boost,
           c_er_stl   = c_exp_stl   + c_e + c_u_stl,
           c_er_gsl   = c_exp_gsl   + c_e + c_u_gsl,
           c_ur_boost = 2*c_u_boost + c_e,
           c_ur_stl   = 2*c_u_stl   + c_e,
           c_ur_gsl   = 2*c_u_gsl   + c_e;

    double sqrtL_boost = std::sqrt(lambertW(2/M_PI*M_E*M_E*std::pow(c_er_boost/c_n_boost, 2))),
           sqrtL_stl   = std::sqrt(lambertW(2/M_PI*M_E*M_E*std::pow(c_er_stl  /c_n_stl  , 2))),
           sqrtL_gsl   = std::sqrt(lambertW(2/M_PI*M_E*M_E*std::pow(c_er_gsl  /c_n_gsl  , 2)));

    double a0_boost = sqrtL_boost - 1/sqrtL_boost,
           a0_stl   = sqrtL_stl   - 1/sqrtL_stl,
           a0_gsl   = sqrtL_gsl   - 1/sqrtL_gsl;

    double astar_boost = astar(c_er_boost, c_sqrt+c_e, c_ur_boost),
           astar_stl   = astar(c_er_stl,   c_sqrt+c_e, c_ur_stl),
           astar_gsl   = astar(c_er_gsl ,  c_sqrt+c_e, c_ur_gsl);

    double astar_boost_f = astar(c_er_boost, c_sqrt_f+c_e_f, c_ur_boost),
           astar_stl_f   = astar(c_er_stl,   c_sqrt_f+c_e_f, c_ur_stl),
           astar_gsl_f   = astar(c_er_gsl,   c_sqrt_f+c_e_f, c_ur_gsl);

    std::cout << "\n\n\nSummary:\n\n" <<
        std::fixed << std::setprecision(4) <<

        "\nOperations:\n\n" <<
      u8"    c_√ (double)         = " << std::setw(8) << c_sqrt << "\n" <<
      u8"    c_e^x (double)       = " << std::setw(8) << c_e    << "\n" <<
      u8"    c_√ + c_e^x (double) = " << std::setw(8) << c_sqrt + c_e << "\n" <<
      "\n" <<
      u8"    c_√ (float)          = " << std::setw(8) << c_sqrt_f << "\n" <<
      u8"    c_e^x (float)        = " << std::setw(8) << c_e_f    << "\n" <<
      u8"    c_√ + c_e^x (float)  = " << std::setw(8) << c_sqrt_f + c_e_f << "\n" <<
        "\n\n";

    constexpr int fieldwidth = 35;
    std::cout << "Draws:" <<
      // 8       4   8       4   8
      std::setw(fieldwidth-2) << ""           << "  boost        stl         gsl\n" <<
        "    " << std::setw(fieldwidth) << "" << " -------     -------     -------\n" <<
        "    " << std::setw(fieldwidth) << std::left << "c_NR = c_HR = c_n" << std::right <<
            std::setw(8) << c_n_boost << "    " <<
            std::setw(8) << c_n_stl << "    " <<
            std::setw(8) << c_n_gsl << "\n" <<
        "    " << std::setw(fieldwidth) << std::left << "c_ER = c_exp + c_e^x + c_u" << std::right <<
            std::setw(8) << c_er_boost << "    " <<
            std::setw(8) << c_er_stl << "    " <<
            std::setw(8) << c_er_gsl << "\n" <<
        "    " << std::setw(fieldwidth) << std::left << "c_UR = 2 c_u + c_e^x" << std::right <<
            std::setw(8) << c_ur_boost << "    " <<
            std::setw(8) << c_ur_stl << "    " <<
            std::setw(8) << c_ur_gsl << "\n" <<

      u8"    a₀ | c_ER, c_HR                    " <<
            std::setw(8) << a0_boost << "    " << std::setw(8) << a0_stl << "    " << std::setw(8) << a0_gsl << "\n\n" <<

      u8"    a* | c_ER, c_UR, c_√, c_e^x        " <<
            std::setw(8) << astar_boost << (astar_boost <= a0_boost ? u8"††  " : "    ") <<
            std::setw(8) << astar_stl   << (astar_stl   <= a0_stl   ? u8"††  " : "    ") <<
            std::setw(8) << astar_gsl   << (astar_gsl   <= a0_gsl   ? u8"††" : "") << "\n";
    if (astar_boost <= a0_boost or astar_stl < a0_stl or astar_gsl < a0_gsl)
        std::cout << std::setw(fieldwidth+4) << "" << u8"††: a* ≤ a₀ ≤ a, so a ≥ a* is trivially satisfied";

    std::cout << "\n\n" <<
      u8"    a* | c_ER, c_UR, c_√(f), c_e^x(f)  " <<
            std::setw(8) << astar_boost_f << (astar_boost_f <= a0_boost ? u8"††  " : "    ") <<
            std::setw(8) << astar_stl_f   << (astar_stl_f   <= a0_stl   ? u8"††  " : "    ") <<
            std::setw(8) << astar_gsl_f   << (astar_gsl_f   <= a0_gsl  ? u8"††\n" : "\n");
    if (astar_boost_f <= a0_boost or astar_stl_f < a0_stl or astar_gsl_f < a0_gsl)
        std::cout << std::setw(fieldwidth+4) << "" << u8"††: a* ≤ a₀ ≤ a, so a ≥ a* is trivially satisfied";

    gsl_rng_free(rng_gsl);
    std::cout << "\n\n\n";
}
