#include <eris/random/rng.hpp>
#include <eris/random/truncated_distribution.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <eris/random/normal_distribution.hpp>
#include <eris/random/halfnormal_distribution.hpp>
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

// Shift the distribution parameters by this tiny amount so that internal calculations won't be
// trivial operations involving 1 or 0.
constexpr double approx_zero = 1e-12, approx_one = 1 + 1e-12;

int main(int argc, char *argv[]) {
    auto &rng = eris::random::rng();

    std::string th_mean, th_var, th_skew, th_kurt;
    std::function<double()> gen;
    int nextarg = 1;
    if (argc > 1) {
        std::string which(argv[nextarg++]);
        if (which == "N") {
            gen = [&rng]() { return eris::random::normal_distribution<double>(approx_zero, approx_one)(rng); };
            th_mean = "0"; th_var = "1"; th_skew = "0"; th_kurt = "3";
            std::cout << "Drawing from N(0,1)\n";
        }
        else if (which == "TN" or which == "TNG") {
            if (argc > nextarg+1) {
                using std::pow;
                double da = std::stod(argv[nextarg++]), db = std::stod(argv[nextarg++]);
                if (which == "TN") {
                    if (da == 0 and std::isinf(db)) {
                        gen = [&rng]() {
                            return eris::random::halfnormal_distribution<double>(approx_zero, approx_one)(rng);
                        };
                    }
                    else {
                        gen = [&rng,da,db]() {
                            return eris::random::truncated_normal_distribution<double>(approx_zero, approx_one, da, db)(rng);
                        };
                    }
                }
                else { // TNG: i.e. the generic version
                    ;
                    boost::math::normal_distribution<double> dnorm(approx_zero, approx_one);
                    gen = [=]() {
                        eris::random::normal_distribution<double> rnorm(approx_zero, approx_one);
                        return eris::random::truncDist(dnorm, rnorm, da, db, approx_zero);
                    };
                }
                precise a = da, b = db;
                // Work around boost bug #12112
                if (std::isinf(da) and da < 0 and a > 0) a = -a;

                // Use extra precision here because we can generate values that are sufficient to
                // underflow some of the below calculations when using doubles.
                // (Note also that we aren't applying the shift here, because it's small enough that
                // it shouldn't really affect the results relative the the sampling noise).
                boost::math::normal_distribution<precise> N01;
                complement(N01, a);
                complement(N01, b);
                precise Z = a >= 0 ? cdf(complement(N01, a)) - cdf(complement(N01, b)) :
                    cdf(N01, b) - cdf(N01, a);
                precise za = pdf(N01, a) / Z, zb = pdf(N01, b) / Z;
                precise mean = za - zb;
                precise mean2 = mean*mean;
                precise mean3=mean2*mean;
                precise mean4=mean2*mean2;
                precise aza = 0.0, a2za = 0.0, a3za = 0.0, bzb = 0.0, b2zb = 0.0, b3zb = 0.0;
                if (za > 0.0) { aza = a*za; a2za = a*aza; a3za = a*a2za; }
                if (zb > 0.0) { bzb = b*zb; b2zb = b*bzb; b3zb = b*b2zb; }
                precise V = 1.0 + aza - bzb - mean2;
                precise s = -pow(V, -1.5) * (-2.0 * mean3 + -(3*bzb - 3*aza - 1.0)*mean + (b2zb - a2za));
                precise k = 1.0/(V*V) * (-3.0*mean4 - 6.0*(bzb - aza)*mean2 - 2.0*mean2
                        + 4.0*(b2zb - a2za)*mean - 3.0*(bzb - aza) - (b3zb - a3za) + 3.0);
                { std::ostringstream ss; ss.precision(10); ss << mean; th_mean = ss.str(); }
                { std::ostringstream ss; ss.precision(10); ss << V; th_var = ss.str(); }
                { std::ostringstream ss; ss.precision(10); ss << s; th_skew = ss.str(); }
                { std::ostringstream ss; ss.precision(10); ss << k; th_kurt = ss.str(); }
                std::cout << "Drawing from TN(0,1,["<< a << "," << b << "])";
                if (which == "TNG") std::cout << " using generic (instead of specialized) algorithm";
                std::cout << std::endl;
            }
            else {
                std::cerr << "Error: TN requires A (min) and B (max) parameters\n";
            }
        }
        else if (which == "U") {
            gen = [&rng]() { return boost::random::uniform_real_distribution<double>(approx_zero, approx_one)(rng); };
            th_mean = "0.5"; th_var = "0.08333..."; th_skew = "0"; th_kurt = "1.8";
            std::cout << "Drawing from U[0,1)\n";
        }
        else if (which == "E") {
            gen = [&rng]() { return eris::random::exponential_distribution<double>(approx_one)(rng); };
            th_mean = "1"; th_var = "1"; th_skew = "2"; th_kurt = "9";
            std::cout << "Drawing from Exp(1)\n";
        }
        else {
            std::cerr << "Unknown distribution `" << which << "'\n";
        }
    }

    bool bad_args = not gen;

    uint64_t count = 10000000;
    if (nextarg < argc and std::regex_match(argv[nextarg], std::regex("\\d+"))) {
        count = std::stoull(argv[nextarg++]);
    }

    for (int i = nextarg; i < argc; i++) {
        std::cerr << "Unknown argument: " << argv[i] << "\n";
        bad_args = true;
    }

    if (bad_args) {
        std::cerr << "Usage: " << argv[0] << " {E|N|U|TN[G] A B} [COUNT] -- draw and report summary stats (mean/variance/skewness/kurtosis) of COUNT (default 10 million) draws from a distribution\n";
        std::cerr << "\nDistributions:\n" <<
            "\tE - Exponential(1)\n" <<
            "\tN - Normal(0,1)\n" <<
            "\tU - Uniform[0,1)\n" <<
            "\tTN - Normal(0,1) truncated to the given [A,B]\n" <<
            "\tTNG - same as TN, but uses a generic, inverse-cdf truncated distribution generator\n";

        exit(1);
    }

    std::cout << "Using seed: " << eris::random::seed() << " (set environment variable ERIS_RNG_SEED to override)\n";
    std::cout.precision(std::numeric_limits<double>::max_digits10);

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
    std::cout << "Mean: " << mean << " (theory: " << th_mean << ")" << std::endl;

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
    double kurt = e4 / (e2*e2);
    std::cout <<
        "Variance: " << var << " (theory: " << th_var << ")\n" <<
        "Skewness: " << skew << " (theory: " << th_skew << ")\n" <<
        "Kurtosis: " << kurt << " (theory: " << th_kurt << ")\n";

}
