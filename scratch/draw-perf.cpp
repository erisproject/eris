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
#include <regex>
extern "C" {
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_cdf.h>
}


// This script computes the time required for various calculations including random draws, then uses
// these to calculate optimal threshold values for an mixed rejection sampling algorithm to draw
// truncated normal variates.
//
// Adding new library calculations for the uniform, exponential, and normal draws is relatively
// straightforward, you just need to:
// - add the necessary headers, above.
// - if the library requires runtime linking, update CMakeLists.txt appropriately.
// - add the name, e.g. "somelib", to the "rng_libs" variable below
// - add a method benchmarkSomeLibrary() which does basically what benchmarkStl() and
//   benchmarkBoost() do, but using calls to your library.  The critical part is to make sure the
//   c["somelib"]["N"], c["somelib"]["U"], and c["somelib"]["Exp"] values are set.  Keep the print
//   statements and mean calculations: they are there to prevent the compiler from being to optimize
//   away the result.
// - inside main(), look for the call to benchmarkStl(), and add a call to benchmarkSomeLibrary()

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
// The libraries (in the order we want to display them).  Keep these names 11 characters or less
// (otherwise the table headers will be misaligned).
std::vector<std::string> rng_libs = {"stl", "boost", "gsl-zigg", "gsl-ratio", "gsl-BoxM", "fairytale"};
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
    // This should never be true, but the compiler shouldn't be able to figure that out.
    // (This should be impossible since doubles should always be 8-byte aligned; but even if not,
    // this is extraordinarily unlikely)
    volatile bool always_false = ((int64_t) &seconds) == 123456789;

    calls_result<decltype(callable())> ret = {0, 0, 0};
    auto start = clk::now();
    do {
        for (unsigned i = 0; i < incr; i++) {
            auto intermediate = callable();
            if (always_false) intermediate = 123.456;
            ret.mean += intermediate;
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

    double &overhead = (std::is_same<decltype(c()), float>::value ? benchmark_overhead_f : benchmark_overhead);
    if (not std::isnan(overhead)) {
        last_benchmark_ns -= overhead;
    }

    std::cout << std::setw(30) << std::left << name + ":" << std::right << std::fixed << std::setprecision(2) <<
        std::setw(7) << 1000/last_benchmark_ns << " MHz = " << std::setw(8) << last_benchmark_ns << " ns/op\n";
    std::cout.unsetf(std::ios_base::floatfield);
    return result.mean;
}
// Benchmark a RNG distribution by constructing it then calling it a bunch of times
template <typename Dist, typename RNG, typename... Args> typename Dist::result_type benchmarkDraw(const std::string &name, RNG &rng, Args&&... args) {
    Dist dist(std::forward<Args>(args)...);
    return benchmark(name, [&dist,&rng]() -> typename Dist::result_type { return dist(rng); });
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

// Calculates and returns a0, the value of a at which ER is more efficient than HR.  When there is
// no extra cost for the sqrt (required for ER, but only once, i.e. not in the sampling loop), this
// equals: sqrt(L) - 1/sqrt(L), where L = LambertW(e^2 * 2/pi * (c_ER/c_HR)), but with the extra
// constant term, the equation becomes a mess, so we just solve for the root (which will also avoid
// any numerical error in the LambertW calculation).
//
// Parameters:
//
// library - the cost library (e.g. "boost") from which to read RNG cost values.  In particular,
// the following cost values must be set in cost[library]:
//     - "HR" - the cost of half-normal rejection sampling.  Since a half-normal pdf divided by a
//       normal pdf is a constant, half-normal rejection needs no separate rejection draw or
//       calculation, and so this is just the cost of drawing a normal.
//     - "ER" - the cost of an exponential rejection draw (including related acceptance draws and
//       calculations), but not including the sqrt cost of calculating lambda(a)
// The following must also be set in either cost[library] or cost[""] (the former takes precedence,
// and is really only intended for the "fairytale" library to pretend costs are different than they
// actually are) :
//     - "sqrt" - the cost of a sqrt
//
// tol - the relative tolerance desired for the returned value.
//
double a0(const std::string &library, const double tol=1e-12) {
    const double &c_HR = cost[library].at("HR"),
          &c_ER = cost[library].at("ER"),
          &c_sqrt = (cost[library].count("sqrt") ? cost[library].at("sqrt") : c_op.at("sqrt"));
    return root([&c_HR, &c_ER, &c_sqrt](const double a) -> double {
        const double lambda = 0.5*(a + std::sqrt(a*a + 4));
        return
            0.5 * c_HR
            -
            c_ER / (
                    boost::math::constants::root_two_pi<double>() * lambda * std::exp(-0.5*lambda*lambda + lambda*a)
                   )
            - c_sqrt * cdf(complement(N01d, a));
    }, 0, 10, tol);
}

// Calculates the threshold value of a above which we want to use lambda=a instead of
// (a+sqrt(a^2+4)/2) in 1-sided-truncation ER sampling.  In other words, this calculates the point
// where the efficiency gain of using the proper lambda stops exceeding the cost of calculating it
// (which requires, most significantly, a sqrt) in the first place.
//
// Parameters:
//
// library - the cost library (e.g. "boost") from which to read RNG cost values.  In particular,
// the following cost values must be set in cost[library]:
//     - "HR" - the cost of half-normal rejection sampling.  Since a half-normal pdf divided by a
//       normal pdf is a constant, half-normal rejection needs no separate rejection draw or
//       calculation, and so this is just the cost of drawing a normal.
//     - "ER" - the cost of an exponential rejection draw (including related acceptance draws and
//       calculations), but not including the sqrt cost of calculating lambda(a)
// The following must also be set in either cost[library] or cost[""] (the former takes precedence,
// and is really only intended for the "fairytale" library to pretend costs are different than they
// actually are) :
//     - "sqrt" - the cost of a sqrt.  If 0, this function returns infinity.
//
// tol - the relative tolerance desired for the returned value.
//
// FIXME: This isn't quite right for two-sided, because it uses the "ER" cost, which equals the cost
// of one exponential draw, one uniform draw, and one e^x calculation, but that is only the minimum
// for a two-sided draw: the expected cost of a two-sided draw actually requires c_Exp to be
// replaced with c_Exp / p_{Exp <= b}.  That, however, results in a non-trivial equation.  Only the
// plus side, the error from using lambda=a is of the opposite sign of the error of using an
// expected cost that is too low, so it tends to partially cancel out the effect.
//
double a0_simplify(const std::string &library, const double tol=1e-12) {
    const double
          &c_ER = cost[library].at("ER"),
          &c_sqrt = (cost[library].count("sqrt") ? cost[library].at("sqrt") : c_op.at("sqrt"));

    // If we're pretending a sqrt is free, there's no reason not to use it.
    if (c_sqrt <= 0) return std::numeric_limits<double>::infinity();

    return root([&c_ER, &c_sqrt](const double a) -> double {
        const double lambda = 0.5*(a + std::sqrt(a*a + 4));
        return
        c_ER * (
                1.0 / (
                    boost::math::constants::root_two_pi<double>() * cdf(complement(N01d, a))
                    * a * std::exp(0.5*a*a)
                )
                -
                1.0 / (
                    boost::math::constants::root_two_pi<double>() * cdf(complement(N01d, a))
                    * lambda * std::exp(lambda*(a - 0.5*lambda))
                )
               )
            - c_sqrt;
    }, 1e-10, 10, tol);
}

// This returns the value of a at which the benefit of using 1/a as an approximation in the decision
// between ER and UR sampling equals the expected value of the cost increase due to using the
// suboptimal UR when ER would be better.  The approximation is:
//  1/a =~ 2/(a+sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)
// where the extra cost of calculation involves one sqrt and one e^x.
//
// In short, when a is above the returned value, use the approximation to determine the threshold b
// value above which ER is preferred.
//
// The decision threshold value of b is at: b = a + z(a), where z(a) is the function above.
//
// Also note that this calculation doesn't use the cost of a division (which is typically around the
// same as the cost of a square root) because the division in this case can be trivally eliminated by
// converting it to a multiplication as needed. (e.g. instead of b < a + 1/a, calculate (b - a)*a < 1)
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
// The following must also be set in either cost[library] or cost[""] (the former takes precedence,
// and is really only intended for the "fairytale" library to pretend costs are different than they
// actually are) :
//     - "sqrt" or "sqrt(f)" - the cost of a sqrt; the latter is used if float_op is true.
//     - "e^x" or "e^x(f)" - the cost of calculating an exponential; the latter is used if float_op
//     is true.
//
// float_op - if true, use the float costs of sqrt and e^x instead of the double costs to calculate
// the cost savings.
//
// tol - the relative tolerance desired for the returned value.
//
// If the cost of the sqrt and the e^x are 0 (i.e. for the fairytale case), this returns infinity:
// that is, it is always better to calculate the precise value when that calculation is costless.
//
double a1(const std::string &library, bool float_op = false, const double tol=1e-12) {
    const std::string sqrt_key = float_op ? "sqrt(f)" : "sqrt", e_x_key = float_op ? "e^x(f)" : "e^x";
    const double
        &c_sqrt = (cost[library].count(sqrt_key) ? cost[library].at(sqrt_key) : c_op.at(sqrt_key)),
        &c_e_x  = (cost[library].count(e_x_key)  ? cost[library].at(e_x_key)  : c_op.at(e_x_key));
    const double cer_over_cur = cost[library].at("ER")/cost[library].at("UR"),
          ccheck_over_cur = (c_sqrt + c_e_x) / cost[library].at("UR");

    if (ccheck_over_cur <= 0) return std::numeric_limits<double>::infinity();

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
// and half-normal rejection (for the a < a0 case).  That is, using an nth-order Taylor
// approximation to e^x is worthwhile when a is below the value returned from this function.
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
// The following must also be set in either cost[library] or cost[""] (the former takes precedence,
// and is really only intended for the "fairytale" library to pretend costs are different than they
// actually are) :
//     - "e^x" or "e^x(f)" - the cost of calculating an exponential; the latter is used if float_op
//       is true.
//     - "e^x_T{n}", where {n} is replaced with the value of n passed into the function; this is the
//       cost of the nth order Taylor approximation.
//
// float_op - if true, use speed of floating point exp rather than double exp.
//
// tol - the desired tolerance level.
//
// Returns 0 if the cost difference between e^x and e^x_Tn is 0 or negative: i.e. if calculating e^x
// costs no more than calculating the Taylor approximation, using e^x is obviously preferable for
// all a.
double aT(const unsigned n, const std::string &library, bool float_op = false, const double tol=1e-12) {
    const std::string e_x_key = float_op ? "e^x(f)" : "e^x";
    const std::string e_x_Tn_key = "e^x_T" + std::to_string(n);
    const double
        cdiff = (cost[library].count(e_x_key)    ? cost[library].at(e_x_key)    : c_op.at(e_x_key))
              - (cost[library].count(e_x_Tn_key) ? cost[library].at(e_x_Tn_key) : c_op.at(e_x_Tn_key)),
        &chr = cost[library].at("HR"),
        &cur = cost[library].at("UR");

    if (cdiff <= 0) return 0;

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
// The following must also be set in either cost[library] or cost[""] (the former takes precedence,
// and is really only intended for the "fairytale" library to pretend costs are different than they
// actually are) :
//     - "e^x_T$n" and "e^x_T${n-1}", where $n and ${n-1} are replaced using the value of n passed
//       into the function; these are the costs of the nth and (n-1)th order Taylor approximations.
//
// tol the desired tolerance level.
//
// If the cost difference is 0, this returns 0 (i.e. always use the higher approximation order when
// doing so is free).
double aTT1(const unsigned n, const std::string &library, const double tol=1e-12) {
    if (n < 2) throw std::logic_error("aTT1(n, ...) requires n >= 2");
    const std::string e_x_Tn_key  = "e^x_T" + std::to_string(n);
    const std::string e_x_Tn1_key = "e^x_T" + std::to_string(n-1);

    const double
        cdiff = (cost[library].count(e_x_Tn_key)  ? cost[library].at(e_x_Tn_key)  : c_op.at(e_x_Tn_key))
              - (cost[library].count(e_x_Tn1_key) ? cost[library].at(e_x_Tn1_key) : c_op.at(e_x_Tn1_key)),
        &chr = cost[library].at("HR"),
        &cur = cost[library].at("UR");

    if (cdiff <= 0) return 0;

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

// Some constants to use below.  These are declared volatile to prevent the compiler from
// optimizing them away.
volatile const double ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, onehalf = 0.5, eight = 8.,
         e = boost::math::constants::e<double>(), pi = boost::math::constants::pi<double>(),
         piandahalf = 1.5*pi, pointone = 0.1, one = 1.0, onepointfive = 1.5;

volatile const float eightf = 8.f, minustwof = -2.f, tenf = 10.f, minustenf = -10.f, twof = 2.f, onehalff = 0.5f,
         piandahalff = piandahalf, ef = boost::math::constants::e<float>();

#define PRECISE(v) std::setprecision(std::numeric_limits<double>::max_digits10) << v << std::setprecision(6)

void benchmarkCalculations() {
    double mean = 0;
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
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "Testing exp approximations\n" << std::setprecision(8);
    mean = 0;
#define DUMP_MEAN std::cout << "    -> " << PRECISE(mean) << "\n"; mean = 0

    // Calculate the costs of Taylor expansions of order n; 1 is 0 (because 1 is the value 1+x, which is basically free)
    c_op["e^x_T1"] = 0;
    // The "e^x_T$n" values are nth order Taylor expansions of e^x
    // The general pattern here started out as:
    //     1. + x*(1. + x*(1./2. + ...
    // instead of:
    //     1. + x + x*x*(1./2. + ...
    // but that ended up being slightly slower.  In effect, the first one forces:
    // Mult -> Add -> Mult -> Add -> ...
    // while the second is:
    // Mult -> Mult -> Add -> Add
    // which is, apparently, faster.  What's weird is this is faster on the inside, too, where we
    // are actually doing an extra multiplication:
    // -> Mult -> Mult -> Mult -> Add -> Add ->
    // instead of
    // -> Mult -> Add -> Mult -> Add ->
    // ... except on T4, where it's better to not expand the inside term.
    //
    // (These results on my Core i5-2500K CPU)
    //
    mean += benchmark("evaluate (d) e^x_T2(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2); });
    c_op["e^x_T2"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T3(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + 1./6*x); });
    c_op["e^x_T3"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T4(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + x*(1./6 + x*(1./24))); });
    c_op["e^x_T4"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T5(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x)); });
    c_op["e^x_T5"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T6(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720))); });
    c_op["e^x_T6"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T7(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720 + x*(1./5040)))); });
    c_op["e^x_T7"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("evaluate (d) e^x_T8(0.5)", [&]() -> double { double x = onehalf; return 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720 + 1./5040*x + x*x*(1./40320)))); });
    c_op["e^x_T8"] = last_benchmark_ns - benchmark_overhead;

    // Put something here that is essentially impossible, but that the compiler can't tell is
    // impossible at compile time so that the mean accumulation (and thus the returned values and
    // thus the calculations) can't be compiled away
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n"; mean = 0;
    mean += benchmark("evaluate (f) e^x_T2(0.5)", [&]() -> float { float x = onehalff; return 1.f + x + x*x*(1.f/2.f); });
    mean += benchmark("evaluate (f) e^x_T4(0.5)", [&]() -> float { float x = onehalff; return 1.f + x + x*x*(1.f/2.f + x*(1.f/6.f + x*(1.f/24.f))); });
    mean += benchmark("evaluate (f) e^x_T8(0.5)", [&]() -> float { float x = onehalff; return 1.f + x + x*x*(1.f/2.f + 1.f/6.f*x + x*x*(1.f/24.f + 1.f/120.f*x + x*x*(1.f/720.f + 1.f/5040.f*x + x*x*(1.f/40320.f)))); });

    mean += benchmark("evaluate (d) e^x_T4(0.1)", [&]() -> double { double x = pointone; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(0.1)=" << PRECISE(std::exp(0.1)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) e^x_T4(0.5)", [&]() -> double { double x = onehalf; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(0.5)=" << PRECISE(std::exp(0.5)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) e^x_T4(1.0)", [&]() -> double { double x = one; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(1.0)=" << PRECISE(std::exp(1.0)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) e^x_T4(1.5)", [&]() -> double { double x = onepointfive; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(1.5)=" << PRECISE(std::exp(1.5)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) e^x_T4(2.0)", [&]() -> double { double x = two; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x))); });
    std::cout << "exp(2.0)=" << PRECISE(std::exp(2.0)) << "; approximation: "; DUMP_MEAN;
    mean += benchmark("evaluate (d) e^x_T5(2.0)", [&]() -> double { double x = two; return 1. + x*(1. + 1./2.*x*(1. + 1./3.*x*(1. + 1./4.*x*(1. + 1./5.*x)))); });
    std::cout << "exp(2.0)=" << PRECISE(std::exp(2.0)) << "; approximation: "; DUMP_MEAN;

    std::cout.precision(6);

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
    mean += benchmark("evaluate e+([2]*pi)", [&]() -> double { return e+(2.0*pi); });
    mean += benchmark("evaluate e*[0.5]*([2]+pi)", [&]() -> double { return e*0.5*(2.0+pi); });
    mean += benchmark("evaluate [0.5]*(e + sqrt(e^2 + [4]))", [&]() -> double { return 0.5*(e + std::sqrt(e*e + 4)); });
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
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}

void benchmarkBoost() {
    std::cout << "\n";
    // Include these with some large numbers so that we can visually inspect the result: these
    // timings should be essentially identical to the standard argument draws, below.  (If they
    // aren't, investigation is warranted).
    double mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(1e9,2e7)", rng_boost, 1e9, 2e7);
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[1e9,1e10)", rng_boost, 1e9, 1e10);
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(30)", rng_boost, 30);
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    // boost draw benchmarks:
    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<boost::random::normal_distribution<double>>("boost N(0,1)", rng_boost);
    cost["boost"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::uniform_real_distribution<double>>("boost U[0,1)", rng_boost);
    cost["boost"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<boost::random::exponential_distribution<double>>("boost Exp(1)", rng_boost);
    cost["boost"]["Exp"] = last_benchmark_ns - benchmark_overhead;
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}

void benchmarkStl() {
    std::cout << "\n";
    // Include these with some large numbers so that we can visually inspect the result: these
    // timings should be essentially identical to the standard argument draws, below.  (If they
    // aren't, investigation is warranted).
    double mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(1e9,2e7)", rng_stl, 1e9, 2e7);
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[1e9,1e10)", rng_stl, 1e9, 1e10);
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(30)", rng_stl, 30);
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    // stl draw benchmarks:
    std::cout << "\n";
    mean = 0;
    mean += benchmarkDraw<std::normal_distribution<double>>("stl N(0,1)", rng_stl);
    cost["stl"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::uniform_real_distribution<double>>("stl U[0,1)", rng_stl);
    cost["stl"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmarkDraw<std::exponential_distribution<double>>("stl Exp(1)", rng_stl);
    cost["stl"]["Exp"] = last_benchmark_ns - benchmark_overhead;
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}

void benchmarkGsl() {
    std::cout << "\n";
    double mean = 0;
    // Like the boost/stl benchmarks, draw some large values to make sure the timings don't depend
    // on the distribution arguments:
    mean += benchmark("gsl N(1e9,2e7) (Box-Mul.)", [&]() -> double { return 1e9 + gsl_ran_gaussian(rng_gsl, 2e7); });
    mean += benchmark("gsl N(1e9,2e7) (ratio)", [&]() -> double { return 1e9 + gsl_ran_gaussian_ratio_method(rng_gsl, 2e7); });
    mean += benchmark("gsl N(1e9,2e7) (ziggurat)", [&]() -> double { return 1e9 + gsl_ran_gaussian_ziggurat(rng_gsl, 2e7); });
    mean += benchmark("gsl U[1e9,1e10]", [&]() -> double { return gsl_ran_flat(rng_gsl, 1e9, 1e10); });
    mean += benchmark("gsl Exp(1/30)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1./30.); });
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    std::cout << "\n";
    mean = 0;
    // gsl draw benchmarks (we draw using the three different normal draw algorithms for
    // comparison):
    mean += benchmark("gsl N(0,1) (Box-Muller)", [&]() -> double { return gsl_ran_gaussian(rng_gsl, 1); });
    cost["gsl-BoxM"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl N(0,1) (ratio)", [&]() -> double { return gsl_ran_gaussian_ratio_method(rng_gsl, 1); });
    cost["gsl-ratio"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl N(0,1) (ziggurat)", [&]() -> double { return gsl_ran_gaussian_ziggurat(rng_gsl, 1); });
    cost["gsl-zigg"]["N"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl U[0,1]", [&]() -> double { return gsl_ran_flat(rng_gsl, 0, 1); });
    cost["gsl-BoxM"]["U"] = cost["gsl-ratio"]["U"] = cost["gsl-zigg"]["U"] = last_benchmark_ns - benchmark_overhead;
    mean += benchmark("gsl Exp(1)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1); });
    cost["gsl-BoxM"]["Exp"] = cost["gsl-ratio"]["Exp"] = cost["gsl-zigg"]["Exp"] = last_benchmark_ns - benchmark_overhead;
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}

// This isn't really a benchmark: it just sets up the fairytale costs and constants by assuming that
// all numerical calculations (even sqrt and e^x) are free, that draw costs are identical for all
// distributions, and, that pairs of draws (as required for ER and UR) have the same cost as a
// single draw from (any) distribution.
//
// I call this the "fairytale" library for obvious reasons.
//
void benchmarkFairytale() {
    auto &cf = cost["fairytale"];
    // All draws (and pairs of draws) have the same cost:
    for (const auto &dist : {"N", "U", "Exp", "NR", "HR", "ER", "UR"}) cf[dist] = 1;
    // All other operations are free:
    for (const auto &op : {"e^x", "e^x(f)", "sqrt", "sqrt(f)"}) cf[op] = 0;
    //for (unsigned n = 2; n <= 8; n++) cf["e^x_T" + std::to_string(n)] = 0;
    cf["aTn"] = std::numeric_limits<double>::infinity();
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

    // Modern CPUs have a variable clock, and may take a couple seconds to increase to maximum frequency, so
    // run a fake test for a few seconds to (hopefully) get the CPU at full speed.
    std::cout << "Busy-waiting to get CPU at full speed\n";
    callTest([]() -> double { return 1.0; }, 3.0);

    {
        volatile const double __attribute__((aligned(16))) overheadd = 1.25;
        mean += benchmark("overhead (d)", [&]() -> double { return overheadd; });
        benchmark_overhead = last_benchmark_ns;

        volatile const float __attribute__((aligned(16))) overheadf = 1.25;
        mean += benchmark("overhead (f)", [&]() -> float { return overheadf; });
        benchmark_overhead_f = last_benchmark_ns;
    }

    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
    std::cout << "\nNB: all following results are net of the above overhead values.\n\n";

    // NB: square brackets around values below indicate compiler time constants (or, at least,
    // constexprs, which should work the same if the compiler is optimizing)


    benchmarkCalculations();

    benchmarkBoost();
    benchmarkStl();
    benchmarkGsl();

    benchmarkFairytale();

    for (const auto &l : rng_libs) {
        if (l == "fairytale") continue; // fairytale sets its own costs
        cost[l]["HR"] = cost[l]["NR"] = cost[l].at("N");
        cost[l]["ER"] = cost[l].at("Exp") + cost[l].at("U") + c_op.at("e^x");
        cost[l]["UR"] = 2*cost[l].at("U") + c_op.at("e^x");
    }

    unsigned max_aTi = 1;
    for (const auto &l : rng_libs) {
        cost[l]["a0"] = a0(l);
        cost[l]["a0s"] = a0_simplify(l);
        cost[l]["a1"] = a1(l);
        cost[l]["a1(f)"] = a1(l, true);
        for (unsigned i = 1; i <= 8; i++) {
            double &ai = cost[l]["aT" + std::to_string(i)];
            ai = aT(i, l);
            // an is the minimum order Taylor expansion needed (for picking just one Taylor expansion)
            if (cost[l].count("aTn") == 0 and ai > cost[l]["a0"]) {
                cost[l]["aTn"] = i;
            }
            if (i > 1) {
                double aTi = aTT1(i, l);
                cost[l]["aT" + std::to_string(i-1) + "-" + std::to_string(i)] = aTi;
                if (aTi < cost[l]["a0"] and max_aTi < i) max_aTi = i;
            }
        }
    }

    std::cout << "\n\n\nSummary:\n\n" <<
        std::fixed << std::setprecision(4) <<

        "\nOperations:\n\n" <<
      u8"    c_√                  = " << std::setw(8) << c_op["sqrt"] << "\n" <<
      u8"    c_e^x                = " << std::setw(8) << c_op["e^x"] << "\n" <<
      u8"    c_e^x (T2 approx.)   = " << std::setw(8) << c_op["e^x_T2"] << "\n" <<
      u8"    c_√ + c_e^x (double) = " << std::setw(8) << c_op["sqrt"] + c_op["e^x"] << "\n" <<
        "\n\n";

    constexpr int fieldwidth = 35;
    std::cout << "Draws:" << std::setw(fieldwidth-5) << "";
#define FOR_l for (const auto &l : rng_libs)
    FOR_l { std::cout << std::setw(11) << std::right << l << " ";  }
    std::cout << "\n" << std::setw(4+fieldwidth) << "";
    for (unsigned i = 0; i < rng_libs.size(); i++) { std::cout << " -------    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_HR = c_NR = c_N" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("NR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_U" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("U") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_Exp" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("Exp") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_UR = 2 c_U + c_e^x" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("UR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_ER = c_Exp + c_e^x + c_U" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("ER") << "    "; }

    std::cout << u8"\n\n    a₀" << std::setw(fieldwidth) << std::left << u8" | c_ER, c_HR, c_√" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a0") << "    "; }

    std::cout << u8"\n\n    a₀'" << std::setw(fieldwidth-1) << std::left << u8" | c_ER, c_√" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a0s") << "    "; }

    // NB: the string.length()-2 thing here is to fool setw into properly displaying the literal
    // utf8 label
    std::cout << u8"\n\n    " << std::setw(fieldwidth+std::string{u8"₁√"}.length()-2) << std::left << "a₁ | c_ER, c_UR, c_√, c_e^x" << std::right;
    bool show_dagger = false;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1"); if (cost[l].at("a1") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is trivially satisfied";
    show_dagger = false;

    /*
    std::cout << u8"\n\n    " << std::setw(fieldwidth+std::string{u8"₁√"}.length()-2) << std::left << "a₁ | c_ER, c_UR, c_√(f), c_e^x(f)" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1(f)"); if (cost[l].at("a1(f)") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is trivially satisfied";
    show_dagger = false;
    */

    std::cout << u8"\n\n    aₙ " << std::setw(fieldwidth-3) << std::left << "(min. Taylor order)" << std::right;
    unsigned max_an = 0;
    FOR_l {
        const double &an = cost[l].at("aTn");
        std::cout << std::setw(8);
        if (std::isfinite(an)) { std::cout << (unsigned) an; if (an > max_an) max_an = an; }
        else std::cout << an;
        std::cout << "    ";
    }
    for (unsigned i = 1; i <= max_an; i++) {
        std::cout << "\n    a_T" << i << std::setw(fieldwidth-4-(unsigned)std::log10(i)) << "";
        FOR_l { std::cout << std::setw(8); if (i <= cost[l].at("aTn")) std::cout << cost[l].at("aT"+std::to_string(i)); else std::cout << ""; std::cout << "    "; }
    }
    for (unsigned i = 2; i <= max_aTi; i++) {
        std::cout << u8"\n    aT{" << i-1 << "→" << i << "}" << std::setw(fieldwidth-7-(unsigned)std::log10(i)-(unsigned)std::log10(i-1)) << "";
        FOR_l {
            const double &v = cost[l].at("aT" + std::to_string(i-1) + "-" + std::to_string(i));
            std::cout << std::setw(8) << v << (v >= cost[l].at("a0") ? u8"≥a₀ " : "    ");
        }
    }

    std::cout << "\n\nR code for the above (for acceptance-speed.R):\n\n" << std::setprecision(4);
    std::cout << "# Costs and thresholds calculated by draw-perf:\n" <<
       "costs <- list(";
    bool first_lib = true;
    FOR_l {
        if (first_lib) first_lib = false; else std::cout << ",";
        const auto &c = cost[l];
        std::cout << "\n  " << std::regex_replace(l, std::regex("\\W+"), ".") << "=list(";
        bool first = true;
        for (const std::string &op : {"N", "U", "Exp", "e^x", "e^x_T2", "sqrt", "a0", "a0s", "a1"}) {
            if (first) first = false; else std::cout << ", ";
            double val = c.count(op) > 0 ? c.at(op) : cost[""].at(op);
            if (l == "fairytale" and op == "e^x_T2") val = 0;
            std::cout << std::regex_replace(op, std::regex("\\W+"), ".") << "=";
            if (std::isinf(val)) std::cout << (val > 0 ? "Inf" : "-Inf");
           else std::cout << val;
        }
        // The current R code simply assumes a 2nd-order Taylor approximation: add an error message
        // if that won't work for all a < a0 (my testing has yet to reveal such a case with any of
        // the libraries I've tested, but I see no reason why it isn't theoretically possible).
        if (c.at("aTn") > 2 and l != "fairytale") std::cout << ", error=stop(\"Error: 2nd-order Taylor approximation insufficient for some a < a0!\")";
        std::cout << ")";
    }
    std::cout << "\n);\n\n\n";

    gsl_rng_free(rng_gsl);
}
