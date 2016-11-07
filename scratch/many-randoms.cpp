#include <eris/random/rng.hpp>
#include <eris/random/truncated_distribution.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/gamma_distribution.hpp>
#include <boost/random/chi_squared_distribution.hpp>
#include <eris/random/normal_distribution.hpp>
#include <eris/random/exponential_distribution.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <random>
#include <iostream>
#include <functional>
#include <chrono>
#include <regex>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

using precise = boost::multiprecision::cpp_dec_float_100;

#define HAVE_ARGS(N) argc > nextarg + (N-1)
#define NEXT_ARG_D std::stod(argv[nextarg++])
#define THEORY_STATS(mean, var, skew, kurt) { th_mean = mean; th_var = var; th_skew = skew; th_kurt = kurt; }

int main(int argc, char *argv[]) {
    auto &rng = eris::random::rng();

    double th_mean, th_var, th_skew, th_kurt;
    std::function<double()> gen;
    int nextarg = 1;
    if (argc > 1) {
        std::string which(argv[nextarg++]);
        if (which == "N") {
            if (HAVE_ARGS(2)) {
                double mu = NEXT_ARG_D, sigma = NEXT_ARG_D;

                gen = [=,&rng]() { return eris::random::normal_distribution<double>(mu, sigma)(rng); };
                THEORY_STATS(mu, sigma*sigma, 0, 0);
                std::cout << "Drawing from N(" << mu << "," << sigma*sigma << ")\n";
            }
            else {
                std::cerr << "Error: TN requires MU and SIGMA parameters\n";
            }
        }
        else if (which == "TN" or which == "TNG") {
            if (HAVE_ARGS(4)) {
                using std::pow;
                double mu = NEXT_ARG_D, sigma = NEXT_ARG_D, left = NEXT_ARG_D, right = NEXT_ARG_D;
                if (which == "TN") {
                    gen = [&rng,mu,sigma,left,right]() {
                        return eris::random::truncated_normal_distribution<double>(mu, sigma, left, right)(rng);
                    };
                }
                else { // TNG: i.e. the generic version
                    ;
                    boost::math::normal_distribution<double> dnorm(mu, sigma);
                    gen = [=]() {
                        eris::random::normal_distribution<double> rnorm(mu, sigma);
                        return eris::random::truncDist(dnorm, rnorm, left, right, mu);
                    };
                }
                precise a = precise(left - mu) / precise(sigma), b = precise(right - mu) / precise(sigma);
                // Work around boost bug #12112
                if (std::isinf(left) and left < 0 and a > 0) a = -a;

                // Use extra precision here because we can generate values that are sufficient to
                // underflow some of the below calculations when using doubles.
                // (Note also that we aren't applying the shift here, because it's small enough that
                // it shouldn't really affect the results relative the the sampling noise).
                //
                // These formulae come from:
                // Pender, Jamol. "The truncated normal distribution: Applications to queues with
                // impatient customers." Operations Research Letters 43, no. 1 (2015): 40-45.
                //
                boost::math::normal_distribution<precise> N01;
                precise Phidiff = a >= 0
                    ? cdf(complement(N01, a)) - cdf(complement(N01, b))
                    : cdf(N01, b) - cdf(N01, a);
                precise v = precise(sigma) * precise(sigma);
                precise phia = pdf(N01, a), phib = pdf(N01, b);
                precise aphia = phia == 0 ? precise(0) : a*phia, bphib = phib == 0 ? precise(0.0) : b*phib;
                precise phidiff = phia - phib;
                precise M = mu + sigma * phidiff / Phidiff;
                precise h2aphia = phia == 0 ? precise(0) : (a*a - 1) * phia,
                        h2bphib = phib == 0 ? precise(0) : (b*b - 1) * phib,
                        h3aphia = phia == 0 ? precise(0) : a*(a*a - 3) * phia,
                        h3bphib = phib == 0 ? precise(0) : b*(b*b - 3) * phib;
                precise V_over_v = 1.0 + (aphia - bphib) / Phidiff - pow(phidiff/Phidiff, 2);
                precise V = V_over_v * v;
                precise S = (
                                (h2aphia - h2bphib) / Phidiff
                                - 3*( (aphia - bphib) * phidiff ) / pow(Phidiff, 2)
                                + 2*( pow(phidiff/Phidiff, 3) )
                        ) / (
                                pow(V_over_v, 1.5)
                            );
                precise K = (
                    12 * (aphia - bphib) * pow(phidiff,2) / pow(Phidiff,3)
                    -
                    4 * (h2aphia - h2bphib)*(phidiff) / pow(Phidiff, 2)
                    -
                    3 * pow((aphia - bphib)/Phidiff, 2)
                    -
                    6 * pow(phidiff/Phidiff, 4)
                    +
                    (h3aphia - h3bphib) / Phidiff
                ) / pow(V_over_v, 2);

                THEORY_STATS(double(M), double(V), double(S), double(K));
                std::cout << "Drawing from TN(" << mu << "," << sigma*sigma << ",["<< a << "," << b << "])";
                if (which == "TNG") std::cout << " using generic inverse-cdf (instead of specialized) algorithm";
                std::cout << std::endl;
            }
            else {
                std::cerr << "Error: " << which << " requires MU, SIGMA, A (min) and B (max) parameters\n";
            }
        }
        else if (which == "U") {
            if (HAVE_ARGS(2)) {
                double a = NEXT_ARG_D, b = NEXT_ARG_D;

                gen = [=,&rng]() { return boost::random::uniform_real_distribution<double>(a, b)(rng); };
                THEORY_STATS(0.5*(b-a), pow(b-a, 2)/12.0, 0, -1.2);
                std::cout << "Drawing from U[" << a << "," << b << ")\n";
            }
            else {
                std::cerr << "Error: U requires A and B parameters\n";
            }
        }
        else if (which == "E") {
            if (HAVE_ARGS(1)) {
                double lambda = NEXT_ARG_D;

                gen = [=,&rng]() { return eris::random::exponential_distribution<double>(lambda)(rng); };
                THEORY_STATS(1.0/lambda, 1.0/pow(lambda,2), 2, 6);
                std::cout << "Drawing from Exp(" << lambda << ")\n";
            }
            else {
                std::cerr << "Error: E requires LAMBDA parameter\n";
            }
        }
        else if (which == "Chi2") {
            if (HAVE_ARGS(1)) {
                double k = NEXT_ARG_D;

                gen = [=,&rng]() { return boost::random::chi_squared_distribution<double>(k)(rng); };
                THEORY_STATS(k, 2*k, sqrt(8/k), 12/k);
                std::cout << "Drawing from Chi^2(" << k << ")\n";
            }
            else {
                std::cerr << "Error: Chi2 requires K parameter\n";
            }
        }
        else if (which == "G") {
            if (HAVE_ARGS(2)) {
                double k = NEXT_ARG_D, theta = NEXT_ARG_D;
                gen = [=,&rng]() { return boost::random::gamma_distribution<double>(k, theta)(rng); };
                THEORY_STATS(k*theta, k*theta*theta, 2/sqrt(k), 6/k);
                std::cout << "Drawing from Gamma(" << k << "," << theta << ")\n";
            }
            else {
                std::cerr << "Error: G requires K and THETA parameters\n";
            }
        }
        else {
            std::cerr << "Unknown distribution `" << which << "'\n";
        }
    }

    bool bad_args = not gen;

    bool time_mode = true;
    double at_least = 1.0;
    uint64_t remaining = 0;
    if (HAVE_ARGS(1) and std::regex_match(argv[nextarg], std::regex("[1-9]\\d*"))) {
        remaining = std::stoull(argv[nextarg++]);
        time_mode = false;
    }
    else if (HAVE_ARGS(1) and std::regex_match(argv[nextarg], std::regex("(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?s"))) {
        at_least = std::stod(argv[nextarg++]); // Will ignore the trailing s
        time_mode = true;
    }

    for (int i = nextarg; i < argc; i++) {
        std::cerr << "Unknown argument: " << argv[i] << "\n";
        bad_args = true;
    }

    if (bad_args) {
        std::cerr << "Usage: " << argv[0] << R"( DIST PARAMS... [COUNT | NUMs] -- draw and report summary stats (mean/variance/skewness/kurtosis) of draws from a distribution

Distributions and parameters (parameters are double values, and mandatory):
    E LAMBDA         - Exponential(LAMBDA)
    N MU SIGMA       - Normal(MU,SIGMA^2)
    U A B            - Uniform[A,B)
    TN  MU SIGMA A B - Normal(MU,SIGMA^2) truncated to the given [A,B]
    TNG MU SIGMA A B - same as TN, but uses inverse cdf sampling instead of rejection sampling
    Chi2 K           - Chi^2(K)
    G K THETA        - Gamma(K, THETA)

The number of draws can be specified either as a fixed number of draws or as a number of seconds
(followed by 's') to perform draws for at least the given number of seconds (which may be
fractional).  If omitted, defaults to "1s".

Examples:

    many-randoms N 5 2.5         # draw from a N(5, 2.5²)
    many-randoms TN 0 1 1 3 2.5s # draw from a standard normal, truncated to [1, 3], for at least 2.5 seconds
    many-randoms E 3 1e9         # draw from an Exp(3) distribution one billion times

)";

        exit(1);
    }

    std::cout << "Using seed: " << eris::random::seed() << " (set environment variable ERIS_RNG_SEED to override)\n";
    std::cout.precision(10);

    // The size of the draw buffer; ideally it should be small to fit in the cache, but large enough
    // that the overhead of querying the clock (somewhere around 100ns on my linux system) is
    // insignificant; the fastest draw seems to be around 8ns (on the same system)
    constexpr size_t DRAW_BUFFER = 16384;
    std::vector<double> draws(DRAW_BUFFER);

    double elapsed = 0;
    uint64_t total_draws = 0;
    double m1{0}, m2{0}, m3{0}, m4{0}; // Variables for online mean/var/skew/kurt calculation
    uint64_t mn{0};
    while (time_mode ? elapsed < at_least : remaining > 0) {
        unsigned num = time_mode ? DRAW_BUFFER : std::min(remaining, DRAW_BUFFER);
        auto start = clk::now();
        for (unsigned i = 0; i < num; i++) {
            draws[i] = gen();
        }
        elapsed += dur(clk::now() - start).count();
        total_draws += num;
        remaining -= num;

        // Online update of various quantities needed for mean/var/skew/kurt calculations:
        for (unsigned i = 0; i < num; i++) {
            uint64_t n_old = mn++;
            const auto &x = draws[i];
            double delta = x - m1, delta_n = delta / mn, delta_n2 = delta_n * delta_n;
            double t1 = delta * delta_n * n_old;
            m1 += delta_n;
            m4 += t1 * delta_n2 * (mn*mn - 3*mn + 3) + 6 * delta_n2 * m2 - 4 * delta_n * m3;
            m3 += t1 * delta_n * (mn-2) - 3 * delta_n * m2;
            m2 += t1;
        }
    }

    std::cout << total_draws << " draws finished in " << elapsed << " seconds (" << total_draws/elapsed/1000000 << " Mdraws/s; " << elapsed/total_draws*1e9 << " ns/draw)" << std::endl;

    double mean = m1;
    double var = m2 / total_draws;
    double skew = m3 / (total_draws * std::pow(var, 1.5));
    double kurt = total_draws * m4 / (m2*m2) - 3;
    std::cout <<
        "Mean:         " << mean << " (theory: " << th_mean << ")\n" <<
        "Variance:     " << var << " (theory: " << th_var << ")\n" <<
        "Skewness:     " << skew << " (theory: " << th_skew << ")\n" <<
        "Ex. Kurtosis: " << kurt << " (theory: " << th_kurt << ")\n";

}
