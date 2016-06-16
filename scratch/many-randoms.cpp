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
                std::cerr << "phia=" << phia << ", phib=" << phib << "\n";
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

    uint64_t count = 10000000;
    if (HAVE_ARGS(1) and std::regex_match(argv[nextarg], std::regex("\\d+"))) {
        count = std::stoull(argv[nextarg++]);
    }

    for (int i = nextarg; i < argc; i++) {
        std::cerr << "Unknown argument: " << argv[i] << "\n";
        bad_args = true;
    }

    if (bad_args) {
        std::cerr << "Usage: " << argv[0] << " DIST PARAMS... [COUNT] -- draw and report summary stats (mean/variance/skewness/kurtosis) of COUNT (default 10 million) draws from a distribution\n";
        std::cerr << "\nDistributions and parameters (parameters are double values, and mandatory):\n" <<
            "    E LAMBDA         - Exponential(LAMBDA)\n" <<
            "    N MU SIGMA       - Normal(MU,SIGMA^2)\n" <<
            "    U A B            - Uniform[A,B)\n" <<
            "    TN  MU SIGMA A B - Normal(MU,SIGMA^2) truncated to the given [A,B]\n" <<
            "    TNG MU SIGMA A B - same as TN, but uses inverse cdf sampling instead of rejection sampling\n" <<
            "    Chi2 K           - Chi^2(K)\n" <<
            "    G K THETA        - Gamma(K, THETA)\n";

        exit(1);
    }

    std::cout << "Using seed: " << eris::random::seed() << " (set environment variable ERIS_RNG_SEED to override)\n";
    std::cout.precision(10);

    double mean = 0;
    double elapsed;
    std::vector<double> draws(count);

    auto start = clk::now();
    for (uint64_t i = 0; i < count; i++) {
        draws[i] = gen();
    }
    elapsed = dur(clk::now() - start).count();
    std::cout << count << " draws finished in " << elapsed << " seconds (" << count/elapsed/1000000 << " Mdraws/s)" << std::endl;

    mean = std::accumulate(draws.begin(), draws.end(), 0.0) / count;
    std::cout <<
        "Mean:         " << mean << " (theory: " << th_mean << ")" << std::endl;

    double e2 = 0, e3 = 0, e4 = 0;
    for (uint64_t i = 0; i < count; i++) {
        double demean = draws[i] - mean;
        double demean2 = demean*demean;
        e2 += demean2;
        e3 += demean2*demean;
        e4 += demean2*demean2;
    }
    e2 /= count;
    e3 /= count;
    e4 /= count;

    double var = e2;
    double skew = e3 / std::pow(e2, 1.5);
    double kurt = e4 / (e2*e2) - 3;
    std::cout <<
        "Variance:     " << var << " (theory: " << th_var << ")\n" <<
        "Skewness:     " << skew << " (theory: " << th_skew << ")\n" <<
        "Ex. Kurtosis: " << kurt << " (theory: " << th_kurt << ")\n";

}
