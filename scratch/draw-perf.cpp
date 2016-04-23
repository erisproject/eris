#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/lexical_cast.hpp>
#include <eris/random/normal_distribution.hpp>
#include <eris/random/exponential_distribution.hpp>
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
using RealType = double;

template <typename T>
struct calls_result {
    unsigned long calls;
    double seconds;
    T mean;
};

boost::random::mt19937_64 rng_boost;
std::mt19937_64 rng_stl;
gsl_rng *rng_gsl;

// library => { testname => cost }; if library is "", this is op costs
std::unordered_map<std::string, std::unordered_map<std::string, double>> cost;
// The libraries (in the order we want to display them).  Keep these names 11 characters or less
// (otherwise the table headers will be misaligned).
std::vector<std::string> rng_libs = {"stl", "boost", "boost+", "gsl-zigg", "gsl-ratio", "gsl-BoxM", "fairytale"};
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
// If non-empty, only run benchmark if its name contains one of these strings, OR the force argument
// is given.  The int counts the number of matches (so that we can warn if something never matched).
std::unordered_map<std::string, int> bench_only;
// Benchmark a function by repeatedly calling it for at least incr times until at least `bench_seconds` has elapsed
template <typename Callable> auto benchmark(const std::string &name, const Callable &c, bool force = false) -> decltype(c()) {
    if (not force and not bench_only.empty()) {
        bool found = false;
        for (auto &s : bench_only) {
            if (name.find(s.first) != name.npos) {
                s.second++;
                found = true;
            }
        }
        if (not found) return std::numeric_limits<decltype(c())>::quiet_NaN();
    }
    auto result = callTest(c, bench_seconds);
    last_benchmark_ns = 1000000000*result.seconds / result.calls;

    double &overhead = (std::is_same<decltype(c()), float>::value ? benchmark_overhead_f : benchmark_overhead);
    if (not std::isnan(overhead)) {
        last_benchmark_ns -= overhead;
    }

    std::cout << std::setw(40) << std::left << name + ":" << std::right << std::fixed << std::setprecision(2) <<
        std::setw(7) << 1000/last_benchmark_ns << " MHz = " << std::setw(8) << last_benchmark_ns << " ns/op" << std::endl;
    std::cout.unsetf(std::ios_base::floatfield);
    return result.mean;
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
        const double Phi_minus_a = cdf(complement(N01d, a));
        return
            c_HR / (2*Phi_minus_a)
            -
            c_ER / (
                    boost::math::constants::root_two_pi<double>() * lambda * std::exp(-0.5*lambda*lambda + lambda*a)
                    * Phi_minus_a
                   )
            ;
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
// where the extra cost of calculation involves an unavoidable sqrt, e^x, and division (plus various
// relatively insignificant additions/multiplications).
//
// In short, when a is above the returned value, use the approximation to determine the threshold b
// value above which ER is preferred.
//
// The decision threshold value of b is at: b = a + z(a), where z(a) is the function above.
//
// Also note that this calculation doesn't use the cost of a division (which is typically around the
// same as the cost of a square root) because the division in this case can be trivally eliminated by
// converting it to a multiplication as needed. (e.g. instead of b < a + 1/a, calculate b*a < a*a + 1)
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
//       is true.
//
// float_op - if true, use the float costs of sqrt, e^x, and division instead of the double costs to
// calculate the cost savings.
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
            cer_over_cur *
            (
                std::exp(-0.5*a*a)
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
// n the order of the higher Taylor approximation for approximating e^x
//
// l the lower order of the Tayler approximations; Tn and Tl are compared.
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
double aTTl(const unsigned n, const unsigned l, const std::string &library, const double tol=1e-12) {
    if (n <= l or l < 1) throw std::logic_error("aTT1(n, l, ...) requires n < l >= 1");
    const std::string e_x_Tn_key  = "e^x_T" + std::to_string(n);
    const std::string e_x_Tl_key = "e^x_T" + std::to_string(l);

    const double
        cdiff = (cost[library].count(e_x_Tn_key) ? cost[library].at(e_x_Tn_key) : c_op.at(e_x_Tn_key))
              - (cost[library].count(e_x_Tl_key) ? cost[library].at(e_x_Tl_key) : c_op.at(e_x_Tl_key)),
        &chr = cost[library].at("HR"),
        &cur = cost[library].at("UR");

    if (cdiff <= 0) return 0;

    return root([&chr, &cur, &cdiff, &n, &l](const double a) -> double {
            const double x = 0.5*a*a;
            double xn = 1;
            long fact = 1;
            double approx_n = 1, approx_l = 1;
            for (unsigned i = 1; i <= n; i++) {
                xn *= x;
                fact *= i;
                approx_n += xn / fact;
                if (i <= l) approx_l = approx_n;
            }

            return
                (exp_cost_delta(a, approx_l, chr, cur) - exp_cost_delta(a, approx_n, chr, cur)) - cdiff;
    }, 0.01, 3.0, tol);
}

// Some constants to use below.  These are declared volatile to prevent the compiler from
// optimizing them away.
volatile const double
        ten = 10.0, minusten = -10.0, two = 2.0, minustwo = -2.0, onehalf = 0.5, eight = 8.,
        e = boost::math::constants::e<double>(), pi = boost::math::constants::pi<double>(),
        piandahalf = 1.5*pi, pointone = 0.1, one = 1.0, onepointfive = 1.5;

volatile const float
        eightf = 8.f, minustwof = -2.f, tenf = 10.f, minustenf = -10.f, twof = 2.f, onehalff = 0.5f,
        piandahalff = piandahalf, ef = e, pif = pi;

volatile const bool not_false = true, not_true = false;

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
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    mean = 0;

    // Calculate the costs of Taylor expansions of order n
    //
    // The "e^x_T$n" values are nth order Taylor expansions of e^x
    //
    // At n=5, the calculation pattern changes: up to 5, the 1-step recursive form a+x*(b+x*(c+x*(...)))
    // is faster; after n=5, the two-step recursive form is faster: a + b*x + x*x*(c + d*x + x*x*(...))
    // (at n=5, the speed is essentially identical).
    //
    // I suspect this has to do with SSE optimizations--it can do two double operations at once, but
    // I suppose there is some overhead of switching into SSE mode.
    mean += benchmark("evaluate (d) e^x_T1(0.5)", [&]() -> double { double x = onehalf; return double(1) + x; });
    c_op["e^x_T1"] = last_benchmark_ns;

    mean += benchmark("evaluate (d) e^x_T2(0.5)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2))); });
    c_op["e^x_T2"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T2(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2)); });

    mean += benchmark("evaluate (d) e^x_T3(0.5)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + double(1)/double(6)*x)); });
    c_op["e^x_T3"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T3(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + double(1)/double(6)*x); });

    mean += benchmark("evaluate (d) e^x_T4(0.5)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + x*(double(1)/double(6) + double(1)/double(24)*x))); });
    c_op["e^x_T4"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T4(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + x*(double(1)/double(6)) + x*x*(double(1)/double(24))); });

    mean += benchmark("evaluate (d) e^x_T5(0.5)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + double(1)/double(6)*x + x*x*(double(1)/double(24) + double(1)/double(120)*x)); });
    c_op["e^x_T5"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T5(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + x*(double(1)/double(6) + x*(double(1)/double(24) + double(1)/double(120)*x)))); });

    mean += benchmark("evaluate (d) e^x_T6(0.5)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + double(1)/double(6)*x + x*x*(double(1)/double(24) + double(1)/double(120)*x + x*x*(double(1)/double(720)))); });
    c_op["e^x_T6"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T6(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + x*(double(1)/double(6) + x*(double(1)/double(24) + x*(double(1)/double(120) + + x*(double(1)/double(720))))))); });

    mean += benchmark("evaluate (d) e^x_T7(0.5)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + double(1)/double(6)*x + x*x*(double(1)/double(24) + double(1)/double(120)*x + x*x*(double(1)/double(720) + x*(double(1)/double(5040))))); });
    c_op["e^x_T7"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T7(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + x*(double(1)/double(6) + x*(double(1)/double(24) + x*(double(1)/double(120) + + x*(double(1)/double(720) + x*(double(1)/double(5040)))))))); });

    mean += benchmark("evaluate (d) e^x_T8(0.5)", [&]() -> double { double x = onehalf; return double(1) + x + x*x*(double(1)/double(2) + double(1)/double(6)*x + x*x*(double(1)/double(24) + double(1)/double(120)*x + x*x*(double(1)/double(720) + double(1)/double(5040)*x + x*x*(double(1)/double(40320))))); });
    c_op["e^x_T8"] = last_benchmark_ns;
    mean += benchmark("evaluate (d) e^x_T8(0.5) (alt)", [&]() -> double { double x = onehalf; return double(1) + x*(double(1) + x*(double(1)/double(2) + x*(double(1)/double(6) + x*(double(1)/double(24) + x*(double(1)/double(120) + + x*(double(1)/double(720) + x*(double(1)/double(5040) + x*(double(1)/double(40320))))))))); });

    // Put something here that is essentially impossible, but that the compiler can't tell is
    // impossible at compile time so that the mean accumulation (and thus the returned values and
    // thus the calculations) can't be compiled away
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
    mean = 0;

    // Branching versions of the above that *should* select one value at compile-time and thus be
    // identical to all of the above, if the compiler is doing its job.
#define Tn_branching(which) \
    static_assert(which >= 1 and which <= 8, "Error: requested Taylor expansion order is not implemented!"); \
    return \
        (which == 1) ? 1 + x : \
        (which == 2) ? 1 + x*(1 + x*(1./2)) : \
        (which == 3) ? 1 + x*(1 + x*(1./2 + x*(1./6))) : \
        (which == 4) ? 1 + x*(1 + x*(1./2 + x*(1./6 + 1./24*x))) : \
        (which == 5) ? 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x)) : \
        (which == 6) ? 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720))) : \
        (which == 7) ? 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720 + x*(1./5040)))) : \
        /*which == 8*/ 1 + x + x*x*(1./2 + 1./6*x + x*x*(1./24 + 1./120*x + x*x*(1./720 + 1./5040*x + x*x*(1./40320))));

    constexpr unsigned use_T1 = 1, use_T2 = 2, use_T3 = 3, use_T4 = 4, use_T5 = 5, use_T6 = 6, use_T7 = 7, use_T8 = 8;
    mean += benchmark("evaluate (d) e^x_T1(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T1); });
    mean += benchmark("evaluate (d) e^x_T2(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T2); });
    mean += benchmark("evaluate (d) e^x_T3(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T3); });
    mean += benchmark("evaluate (d) e^x_T4(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T4); });
    mean += benchmark("evaluate (d) e^x_T5(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T5); });
    mean += benchmark("evaluate (d) e^x_T6(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T6); });
    mean += benchmark("evaluate (d) e^x_T7(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T7); });
    mean += benchmark("evaluate (d) e^x_T8(0.5) (ccbr.)", [&]() -> double { double x = onehalf; Tn_branching(use_T8); });

    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
    mean = 0;
    // The above are actually the Maclaurin series (i.e. approximated at a=0), but since we're going
    // to be using this for the range [0, a0], we could also try evaluating the approximation in the
    // middle of this range.
    constexpr double exp_T_a = 0.25;
    constexpr double exp_at_a = 1.284025416687741484073420568062436458336;
    mean += benchmark("evaluate (d) e^x_T2@a=.25(0.5)", [&]() -> double { double x = onehalf; double x_m_a = x-exp_T_a; return exp_at_a + exp_at_a*x_m_a + 0.5*exp_at_a*x_m_a*x_m_a; });
    mean += benchmark("evaluate (d) e^x_T3@a=.25(0.5)", [&]() -> double { double x = onehalf; return exp_at_a*(1 + (x-exp_T_a) + (x-exp_T_a)*(x-exp_T_a)*(1./2 + 1./6*(x-exp_T_a))); });

    mean += benchmark("evaluate (d) log(10)", [&]() -> double { return std::log(ten); });
    mean += benchmark("evaluate (d) log(piandahalf)", [&]() -> double { return std::log(piandahalf); });
    mean += benchmark("evaluate (d) log(e)", [&]() -> double { return std::log(e); });
    mean += benchmark("evaluate (f) log(10)", [&]() -> float { return std::log(tenf); });
    mean += benchmark("evaluate (f) log(piandahalf)", [&]() -> float { return std::log(piandahalff); });
    mean += benchmark("evaluate (f) log(e)", [&]() -> float { return std::log(ef); });

    mean += benchmark("evaluate sqrt(8)", [&]() -> double { return std::sqrt(eight); });
    mean += benchmark("evaluate sqrt(1.5pi)", [&]() -> double { return std::sqrt(piandahalf); });
    c_op["sqrt"] = last_benchmark_ns;
    mean += benchmark("evaluate (f) sqrt(8)", [&]() -> float { return std::sqrt(eightf); });
    mean += benchmark("evaluate (f) sqrt(1.5pi)", [&]() -> float { return std::sqrt(piandahalff); });
    c_op["sqrt(f)"] = last_benchmark_ns;

    mean += benchmark("evaluate [1]/pi", [&]() -> double { return 1.0/pi; });
    c_op["/"] = last_benchmark_ns;
    mean += benchmark("evaluate [1]/sqrt(pi)", [&]() -> double { return 1.0/std::sqrt(pi); });
    mean += benchmark("evaluate sqrt([1]/pi)", [&]() -> double { return std::sqrt(1.0/pi); });

    mean += benchmark("evaluate (f) [1]/pi", [&]() -> float { return 1.0f/pi; });
    c_op["/(f)"] = last_benchmark_ns;
    mean += benchmark("evaluate (f) [1]/sqrt(pi)", [&]() -> float { return 1.0f/std::sqrt(pif); });
    mean += benchmark("evaluate (f) sqrt([1]/pi)", [&]() -> float { return std::sqrt(1.0f/pif); });

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

// These rejection sampling benchmark macros are designed to test the speed of rejection sampling.
// We do this by testing the rejection sampling over a range where each type should generate an
// acceptable draw with probability 0.9, then we scale the final speed by 0.9 to get an estimate of
// the speed of a single (whether accepted or rejected) draw. (this works because the mean number
// of draws will be 1/0.9).

//Usage BENCH_NR_START("display name"); ... pre-loop initialization code ...; BENCH_NR_END("cost-key", somecall())
// somecall() can make use of _mean and _sigma, should return the proper normal.
#define BENCH_NR_START(name) \
    mean += benchmark(name, [&]() -> double { \
            RealType x; \
            const RealType _mean = 0.2, _sigma = 0.1, _upper_limit = 0.3466299, _lower_limit = 0.01;
#define BENCH_NR_END(lib, draw_call) \
            do { x = draw_call; } while (x < _lower_limit or x > _upper_limit); \
            return x; \
    }); \
    cost[lib]["NR"] = last_benchmark_ns * 0.9;
// Macro for doing the above when no pre-loop initialization is needed
#define BENCH_NR(name, lib, draw_call) BENCH_NR_START(name) BENCH_NR_END(lib, draw_call)

// Same as above, but for half-normal; draw_call should return a standard normal
#define BENCH_HR_START(name) \
    mean += benchmark(name, [&]() -> double { \
        const RealType _mean = 0.2, _sigma = 0.1, _upper_limit = 0.3879895, _lower_limit = .205; \
        const RealType signed_sigma(not_true ? -_sigma : _sigma); \
        RealType x;
#define BENCH_HR_END(lib, draw_std_normal) \
        do { x = _mean + signed_sigma*fabs(draw_std_normal); } while (x < _lower_limit or x > _upper_limit); \
        return x; \
    }); \
    cost[lib]["HR"] = last_benchmark_ns * 0.9;
#define BENCH_HR(name, lib, draw_std_normal) BENCH_HR_START(name) BENCH_HR_END(lib, draw_std_normal)

// Same as above, but for half-normal; draw_exp should return an exponential(1) draw.
#define BENCH_ER_START(name) \
    mean += benchmark(name, [&]() -> double { \
        const RealType _sigma = 0.1; \
        const RealType _upper_limit = std::numeric_limits<RealType>::infinity(), _lower_limit = .373015; \
        const RealType a = _lower_limit - 0.1; \
        const RealType exp_max_times_sigma = _upper_limit - _lower_limit; \
        const RealType x_scale = _sigma / a; \
        const RealType x_delta = 0; \
        RealType x;
#define BENCH_ER_END(lib, draw_exp) \
        do { \
            do { x = (draw_exp) * x_scale; } while (_sigma * x > exp_max_times_sigma); \
        } while (2 * (draw_exp) <= (x+x_delta)*(x+x_delta)); \
        return not_true ? _upper_limit - x*_sigma : _lower_limit + x*_sigma; \
    }); \
    cost[lib]["ER"] = last_benchmark_ns * 0.9;
#define BENCH_ER(name, lib, draw_exp) BENCH_ER_START(name) BENCH_ER_END(lib, draw_exp)

// Likewise, for UR
#define BENCH_UR_START(name) \
    mean += benchmark(name, [&]() -> double { \
        RealType x, rho; \
        const RealType _upper_limit = 0.15205581, _lower_limit = 0.13693365; \
        const RealType _ur_inv_2_sigma_squared = 50; \
        const RealType _ur_shift = _lower_limit*_lower_limit; \
        const RealType _mean = 0;
#define BENCH_UR_END(lib, draw_x, test_rho) \
        do { \
            x = (draw_x); \
            rho = std::exp(_ur_inv_2_sigma_squared * (_ur_shift - (x - _mean)*(x - _mean))); \
        } while (test_rho); \
        return x; \
    }); \
    cost[lib]["UR"] = last_benchmark_ns * 0.9;
#define BENCH_UR(name, lib, draw_x, draw_rho) BENCH_UR_START(name) BENCH_UR_END(lib, draw_x, draw_rho)

template <
class Normal = boost::random::normal_distribution<double>,
class Exponential = boost::random::exponential_distribution<double>,
class Uniform = boost::random::uniform_real_distribution<double>,
class Unif01 = boost::random::uniform_01<double>
>
void benchmarkBoost(const std::string &key = "boost") {
    std::cout << "\n";
    // Include these with some large numbers so that we can visually inspect the result: these
    // timings should be essentially identical to the standard argument draws, below.  (If they
    // aren't, investigation is warranted).
    double mean = 0;
    mean += benchmark(key + " N(1e9,2e7)", []() -> double { return Normal(1e9, 2e7)(rng_boost); });
    mean += benchmark(key + " U[1e9,1e10)", []() -> double { return Uniform(1e9, 1e10)(rng_boost); });
    mean += benchmark(key + " Exp(30)", []() -> double { return Exponential(30)(rng_boost); });
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    // boost draw benchmarks:
    std::cout << "\n";
    mean = 0;

    // Constructing on-the-fly vs preconstructing seems basically the same speed for N and Exp, and
    // slightly faster for uniform, so we'll use the non-preconstruction for timing calculations.
    mean += benchmark(key + " N(0,1) (incl. construction)", []() -> double { return Normal(0, 1)(rng_boost); });
    cost[key]["N"] = last_benchmark_ns;
    mean += benchmark(key + " U[0,1] (incl. construction)", []() -> double { return Uniform(0,1)(rng_boost); });
    cost[key]["U"] = last_benchmark_ns;
    mean += benchmark(key + " U01 (incl. construction)", []() -> double { return Unif01()(rng_boost); });
    mean += benchmark(key + " Exp(1) (incl. construction)", []() -> double { return Exponential(1)(rng_boost); });
    cost[key]["Exp"] = last_benchmark_ns;
    Normal rnorm(0, 1);
    Uniform runif(0, 1);
    Exponential rexp(1);
    mean += benchmark(key + " N(0,1) (pre-constructed)", [&rnorm]() -> double { return rnorm(rng_boost); });
    mean += benchmark(key + " U[0,1] (pre-constructed)", [&runif]() -> double { return runif(rng_boost); });
    mean += benchmark(key + " Exp(1) (pre-constructed)", [&rexp]() -> double { return rexp(rng_boost); });


    BENCH_NR(key + " NR", key, Normal(_mean, _sigma)(rng_boost));

    BENCH_HR(key + " HR", key, Normal()(rng_boost));

    BENCH_ER_START(key + " ER");
    Exponential exponential;
    BENCH_ER_END(key, exponential(rng_boost));

    BENCH_UR(key + " UR", key,
            Uniform(_lower_limit, _upper_limit)(rng_boost),
            Unif01()(rng_boost) > rho);

    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}


void benchmarkStl() {
    std::cout << "\n";
    // Include these with some large numbers so that we can visually inspect the result: these
    // timings should be essentially identical to the standard argument draws, below.  (If they
    // aren't, investigation is warranted).
    double mean = 0;
    mean += benchmark("stl N(1e9,2e7)", []() -> double { return std::normal_distribution<double>(1e9, 2e7)(rng_stl); });
    mean += benchmark("stl U[1e9,1e10)", []() -> double { return std::uniform_real_distribution<double>(1e9, 1e10)(rng_stl); });
    mean += benchmark("stl Exp(30)", []() -> double { return std::exponential_distribution<double>(30)(rng_stl); });
    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";

    // stl draw benchmarks:
    std::cout << "\n";
    mean = 0;

    // Like boost, on-the-fly is just as fast as pre-constructing for uniform and exp, but about
    // half the speed for normal: libstdc++ generates normals in pairs, so every second call is
    // essentially free.  So we'll use on-the-fly for uniform and exponential timings,
    // preconstruction for normal timings.
    mean += benchmark("stl N(0,1) (incl. construction)", []() -> double { return std::normal_distribution<double>(0, 1)(rng_stl); });
    mean += benchmark("stl U[0,1] (incl. construction)", []() -> double { return std::uniform_real_distribution<double>(0,1)(rng_stl); });
    cost["stl"]["U"] = last_benchmark_ns;
    mean += benchmark("stl Exp(1) (incl. construction)", []() -> double { return std::exponential_distribution<double>(1)(rng_stl); });
    cost["stl"]["Exp"] = last_benchmark_ns;

    std::normal_distribution<double> rnorm(0, 1);
    std::uniform_real_distribution<double> runif(0, 1);
    std::exponential_distribution<double> rexp(1);
    mean += benchmark("stl N(0,1) (pre-constructed)", [&rnorm]() -> double { return rnorm(rng_stl); });
    cost["stl"]["N"] = last_benchmark_ns;
    mean += benchmark("stl U[0,1] (pre-constructed)", [&runif]() -> double { return runif(rng_stl); });
    mean += benchmark("stl Exp(1) (pre-constructed)", [&rexp]() -> double { return rexp(rng_stl); });

    BENCH_NR("stl NR", "stl", _mean + _sigma*rnorm(rng_stl));

    BENCH_HR("stl HR", "stl", rnorm(rng_stl));

    BENCH_ER_START("stl ER");
    std::exponential_distribution<RealType> exponential;
    BENCH_ER_END("stl", exponential(rng_stl));

    BENCH_UR("stl UR", "stl",
            std::uniform_real_distribution<RealType>(_lower_limit, _upper_limit)(rng_stl),
            std::uniform_real_distribution<RealType>()(rng_stl) > rho);

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

    // Box-Muller is half the speed it could be: gsl discards the second value instead of saving it
    // (like libstdc++ does).
    mean += benchmark("gsl N(0,1) (Box-Muller)", [&]() -> double { return gsl_ran_gaussian(rng_gsl, 1); });
    cost["gsl-BoxM"]["N"] = last_benchmark_ns;
    mean += benchmark("gsl N(0,1) (ratio)", [&]() -> double { return gsl_ran_gaussian_ratio_method(rng_gsl, 1); });
    cost["gsl-ratio"]["N"] = last_benchmark_ns;
    mean += benchmark("gsl N(0,1) (ziggurat)", [&]() -> double { return gsl_ran_gaussian_ziggurat(rng_gsl, 1); });
    cost["gsl-zigg"]["N"] = last_benchmark_ns;
    mean += benchmark("gsl U[0,1]", [&]() -> double { return gsl_ran_flat(rng_gsl, 0, 1); });
    cost["gsl-BoxM"]["U"] = cost["gsl-ratio"]["U"] = cost["gsl-zigg"]["U"] = last_benchmark_ns;
    mean += benchmark("gsl Exp(1)", [&]() -> double { return gsl_ran_exponential(rng_gsl, 1); });
    cost["gsl-BoxM"]["Exp"] = cost["gsl-ratio"]["Exp"] = cost["gsl-zigg"]["Exp"] = last_benchmark_ns;

    BENCH_NR("gsl NR (ziggurat)", "gsl-zigg", _mean + gsl_ran_gaussian_ziggurat(rng_gsl, _sigma));
    BENCH_HR("gsl HR (ziggurat)", "gsl-zigg", gsl_ran_gaussian_ziggurat(rng_gsl, 1));

    BENCH_NR("gsl NR (ratio)", "gsl-ratio", _mean + gsl_ran_gaussian_ratio_method(rng_gsl, _sigma));
    BENCH_HR("gsl HR (ratio)", "gsl-ratio", gsl_ran_gaussian_ratio_method(rng_gsl, 1));

    BENCH_NR("gsl NR (Box-Muller)", "gsl-BoxM", _mean + gsl_ran_gaussian(rng_gsl, _sigma));
    BENCH_HR("gsl HR (Box-Muller)", "gsl-BoxM", gsl_ran_gaussian(rng_gsl, 1));

    BENCH_ER("gsl ER", "gsl-zigg", gsl_ran_exponential(rng_gsl, 1));

    BENCH_UR("gsl UR", "gsl-zigg", gsl_ran_flat(rng_gsl, _lower_limit, _upper_limit), gsl_ran_flat(rng_gsl, 0, 1) > rho);

    for (const auto &r : {"ER", "UR"}) for (const auto &g : {"gsl-BoxM", "gsl-ratio"})
        cost[g][r] = cost["gsl-zigg"][r];

    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
}

// This isn't really a benchmark: it just sets up the fairytale costs and constants by assuming that
// all numerical calculations (even sqrt and e^x) are free, that draw costs are identical for all
// distributions, and, that *pairs* of draws (as required for ER and UR) have the same cost as a
// single draw from (any) distribution.
//
// I call this the "fairytale" library for obvious reasons.
//
void benchmarkFairytale() {
    auto &cf = cost["fairytale"];
    // All draws (and *pairs* of draws, for rejection sampling) have the same cost:
    for (const auto &dist : {"N", "U", "Exp", "NR", "HR", "ER", "UR"}) cf[dist] = 1;
    // All other operations are free:
    for (const auto &op : {"e^x", "e^x(f)", "sqrt", "sqrt(f)", "/", "/(f)"}) cf[op] = 0;
    //for (unsigned n = 2; n <= 8; n++) cf["e^x_T" + std::to_string(n)] = 0;
    cf["aTmin"] = std::numeric_limits<double>::infinity();
}


void printSummary() {

    unsigned max_aTi = 1;
    for (const auto &l : rng_libs) {
        cost[l]["a0"] = a0(l);
        cost[l]["a0s"] = a0_simplify(l);
        cost[l]["a1"] = a1(l);
        cost[l]["a1(f)"] = a1(l, true);
        cost[l]["b1"] = boost::math::constants::root_two_pi<double>() * cost[l].at("NR") / cost[l].at("UR");
        for (unsigned i = 1; i <= 8; i++) {
            double &ai = cost[l]["aTlim" + std::to_string(i)];
            ai = aT(i, l);
            // an is the minimum order Taylor expansion needed (for picking just one Taylor expansion)
            if (cost[l].count("aTmin") == 0 and ai > cost[l]["a0"]) {
                cost[l]["aTmin"] = i;
            }
            for (unsigned j = i-1; j > 0; j--) {
                double aTj = aTTl(i, j, l);
                if (aTj == 0) continue;
                cost[l]["aT" + std::to_string(i)] = aTj;
                if (aTj < cost[l]["a0"] and max_aTi < i) max_aTi = i;
                break;
            }
        }
    }


    std::cout << "\n\n\nSummary:\n\n" <<
        std::fixed << std::setprecision(4) <<

        "\nOperations:\n\n" <<
      u8"    c_√                  = " << std::setw(8) << c_op["sqrt"] << "\n" <<
      u8"    c_/                  = " << std::setw(8) << c_op["/"] << "\n" <<
      u8"    c_e^x                = " << std::setw(8) << c_op["e^x"] << "\n" <<
      u8"    c_e^x (T2 approx.)   = " << std::setw(8) << c_op["e^x_T2"] << "\n" <<
      u8"    c_√ + c_e^x + c_/    = " << std::setw(8) << c_op["sqrt"] + c_op["e^x"] + c_op["/"] << "\n" <<
        "\n\n";

    constexpr int fieldwidth = 35;
    std::cout << "Draws:" << std::setw(fieldwidth-5) << "";
#define FOR_l for (const auto &l : rng_libs)
    FOR_l { std::cout << std::setw(11) << std::right << l << " ";  }
    std::cout << "\n" << std::setw(4+fieldwidth) << "";
    for (unsigned i = 0; i < rng_libs.size(); i++) { std::cout << " -------    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_N" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("N") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_U" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("U") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_Exp" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("Exp") << "    "; }

    std::cout << "\n\nRejection sampling cost:" << std::setw(fieldwidth-23) << "";
#define FOR_l for (const auto &l : rng_libs)
    FOR_l { std::cout << std::setw(11) << std::right << l << " ";  }
    std::cout << "\n" << std::setw(4+fieldwidth) << "";
    for (unsigned i = 0; i < rng_libs.size(); i++) { std::cout << " -------    "; }

    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_NR =~ c_N" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("NR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_HR =~ c_N" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("HR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_UR =~ 2 c_U + c_e^x" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("UR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_ER =~ 2 c_Exp + c_/" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("ER") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_NR / c_UR" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("NR") / cost[l].at("UR") << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_HR / c_UR" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("HR") / cost[l].at("UR")  << "    "; }
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "c_ER / c_UR" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("ER") / cost[l].at("UR")  << "    "; }

    std::cout << "\n\nCalculation thresholds:" << std::setw(fieldwidth-22) << "";
#define FOR_l for (const auto &l : rng_libs)
    FOR_l { std::cout << std::setw(11) << std::right << l << " ";  }
    std::cout << "\n" << std::setw(4+fieldwidth) << "";
    for (unsigned i = 0; i < rng_libs.size(); i++) { std::cout << " -------    "; }

    std::cout << u8"\n\n    a₀: " << std::setw(fieldwidth-4) << std::left << "hr_below_er_above" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a0") << "    "; }

    std::cout << u8"\n\n    " << std::setw(fieldwidth) << std::left << "simplify_er_lambda_above" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a0s") << "    "; }

    // NB: the string.length()-2 thing here is to fool setw into properly displaying the literal
    // utf8 label
    std::cout << u8"\n\n    a₁: " << std::setw(fieldwidth-4) << std::left << "simplify_er_ur_above" << std::right;
    bool show_dagger = false;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1"); if (cost[l].at("a1") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is always satisfied";
    show_dagger = false;

    // b1: the value of b-a in straddling-0 truncation above which we prefer NR, below which, UR.
    std::cout << "\n    " << std::setw(fieldwidth) << std::left << "ur_below_nr_above" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("b1") << "    "; }

    /*
    std::cout << u8"\n\n    " << std::setw(fieldwidth+std::string{u8"₁√"}.length()-2) << std::left << "a₁ | c_ER, c_UR, c_√(f), c_e^x(f)" << std::right;
    FOR_l { std::cout << std::setw(8) << cost[l].at("a1(f)"); if (cost[l].at("a1(f)") <= cost[l].at("a0")) { std::cout << u8"††  "; show_dagger = true; } else std::cout << "    "; }
    if (show_dagger)
        std::cout << "\n    " << std::setw(fieldwidth) << "" << u8"††: a₁ ≤ a₀ ≤ a, so a ≥ a₁ is trivially satisfied";
    show_dagger = false;
    */

    std::cout << u8"\n\n    " << std::setw(fieldwidth) << std::left << "Min. Taylor order required" << std::right;
    unsigned max_an = 0;
    FOR_l {
        const double &an = cost[l].at("aTmin");
        std::cout << std::setw(8);
        if (std::isfinite(an)) { std::cout << (unsigned) an; if (an > max_an) max_an = an; }
        else std::cout << an;
        std::cout << "    ";
    }
    for (unsigned i = 1; i <= max_an; i++) {
        std::cout << "\n    T" << i << " > e^x for a <" << std::setw(fieldwidth-16-(unsigned)std::log10(i)) << "";
        FOR_l { std::cout << std::setw(8); if (i <= cost[l].at("aTmin")) std::cout << cost[l].at("aTlim"+std::to_string(i)); else std::cout << ""; std::cout << "    "; }
    }
    for (unsigned i = 2; i <= max_aTi; i++) {
        std::cout << u8"\n    T" << i << " preferred for a >" << std::setw(fieldwidth-20-(unsigned)std::log10(i)) << "";
        FOR_l {
            if (cost[l].count("aT" + std::to_string(i))) {
                const double &v = cost[l].at("aT" + std::to_string(i));
                std::cout << std::setw(8) << v << (v >= cost[l].at("a0") ? u8"≥a₀ " : "    ");
            }
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
        for (const std::string &op : {"N", "U", "Exp", "e^x", "e^x_T2", "sqrt", "a0", "a0s", "a1", "b1"}) {
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
        if (c.at("aTmin") > 2 and l != "fairytale") std::cout << ", error=stop(\"Error: 2nd-order Taylor approximation insufficient for some a < a0!\")";
        std::cout << ")";
    }
    std::cout << "\n);\n\n\n";
}


std::regex seconds_re("\\d+\\.?|\\d*\\.\\d+"),
    seed_re("\\d+"),
    help_re("-h|--help|-?");

// Test the draw speed of various distributions
int main(int argc, char *argv[]) {
    // If we're given one argument, it's the number of seconds; if 2, it's seconds and a seed
    unsigned long seed = 0;
    bool need_help = false, saw_seconds = false, saw_seed = false;
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (std::regex_match(arg, help_re)) {
            need_help = true;
        }
        else if (not saw_seconds and std::regex_match(arg, seconds_re)) {
            try {
                bench_seconds = boost::lexical_cast<double>(arg);
            }
            catch (const boost::bad_lexical_cast &) {
                std::cerr << "Invalid SECONDS value `" << arg << "'\n";
                need_help = true;
            }
        }
        else if (not saw_seed and std::regex_match(arg, seed_re)) {
            try {
                seed = boost::lexical_cast<decltype(seed)>(arg);
            }
            catch (const boost::bad_lexical_cast &) {
                std::cerr << "Invalid SEED value `" << arg << "'\n";
                need_help = true;
            }
        }
        else {
            bench_only[arg] = 0;
        }
    }

    if (need_help) {
        std::cerr << "Usage: " << argv[0] << " [SECONDS [SEED]] [TEST ...]\n\n";
        std::cerr << "TEST is one or more substrings to match against test names: if provided, only\n" <<
                     "matching benchmarks will be performed, and summary values will not be calculated\n";
        exit(1);
    }

    if (not saw_seed) {
        std::random_device rd;
        static_assert(sizeof(decltype(rd())) == 4, "Internal error: std::random_device doesn't give 32-bit values!?");
        seed = (uint64_t(rd()) << 32) + rd();
    }
    std::cout << "Using mt19937 generator with seed = " << seed << std::endl;
    std::cout << std::showpoint;
    rng_stl.seed(seed);
    rng_boost.seed(seed);
    rng_gsl = gsl_rng_alloc(gsl_rng_mt19937);
    gsl_rng_set(rng_gsl, seed);

    double mean = 0;

    // Modern CPUs have a variable clock, and may take a couple seconds to increase to maximum frequency, so
    // run a fake test for a few seconds to (hopefully) get the CPU at full speed.
    std::cout << "Busy-waiting to get CPU at full speed" << std::endl;
    callTest([]() -> double { return 1.0; }, 3.0);

    {
        volatile const double __attribute__((aligned(16))) overheadd = 1.25;
        mean += benchmark("overhead (d)", [&]() -> double { return overheadd; }, true);
        benchmark_overhead = last_benchmark_ns;

        volatile const float __attribute__((aligned(16))) overheadf = 1.25;
        mean += benchmark("overhead (f)", [&]() -> float { return overheadf; }, true);
        benchmark_overhead_f = last_benchmark_ns;
    }

    if (mean == -123.456) std::cout << "sum of these means: " << PRECISE(mean) << "\n";
    std::cout << "\nNB: all following results are net of the above overhead values.\n" << std::endl;

    // NB: square brackets around values below indicate compiler time constants (or, at least,
    // constexprs, which should work the same if the compiler is optimizing)


    benchmarkCalculations();

    benchmarkBoost<>();
    benchmarkBoost<eris::random::normal_distribution<double>, eris::random::exponential_distribution<double>>("boost+");
    benchmarkStl();
    benchmarkGsl();

    benchmarkFairytale();

    if (bench_only.empty()) {
        printSummary();
    }
    else {
        for (const auto &b : bench_only) {
            if (b.second == 0) std::cerr << "Warning: `" << b.first << "' didn't match any benchmarks\n";
        }
    }

    gsl_rng_free(rng_gsl);
}
