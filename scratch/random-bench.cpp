#include <eris/random/rng.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <random>
#include <iostream>
#include <functional>
#include <chrono>
#include <regex>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

// Shift the distribution parameters by this tiny amount so that internal calculations won't be
// trivial operations involving 1 or 0.
constexpr double approx_zero = -1e-300, approx_one = 1 + 1e-12;

constexpr unsigned int BENCH_NORMAL = 1, BENCH_HALFNORMAL = 1 << 1, BENCH_UNIFORM  = 1 << 2, BENCH_EXPONENTIAL = 1 << 3;

struct benchmark {
    const double left, right;
    bool normal, halfnormal, uniform, exponential;
    struct {
        std::pair<long, double> normal, halfnormal, uniform, exponential;
    } timing;
    benchmark(double l, double r, bool n = false, bool h = false, bool u = false, bool e = false)
        : left{l}, right{r}, normal{n}, halfnormal{h}, uniform{u}, exponential{e}
    {}
};

double draw_random_parameter() {
    double d =
        boost::random::uniform_01<double>()(eris::random::rng()) < 0.05
        ? std::numeric_limits<double>::infinity()
        : std::exponential_distribution<double>(0.5)(eris::random::rng());

    if (boost::random::uniform_01<double>()(eris::random::rng()) < 0.5)
        d = -d;

    return d;
}

// We store results here, so that they can't be compiled away.
double garbage = 0.0;
// Runs code for at least the given time; returns the number of times run, and the total runtime.
std::pair<long, double> bench(std::function<double()> f, double at_least = 0.25, int increment = 1000000) {
    auto start = clk::now();
    std::pair<long, double> results;
    do {
        for (int i = 0; i < increment; i++) {
            garbage += f();
        }
        results.second = dur(clk::now() - start).count();
        results.first += increment;
    } while (results.second < at_least);

    return results;
}

int main(int argc, char *argv[]) {

    // Draw values randomly.  We do this by drawing infinity with probability 0.2, and otherwise
    // take a draw from exp(.5).  This is then multiplied by a random -1/+1 modifier.  For pair of
    // random parameters so obtained, we figure out which rejection methods are suitable according
    // to:
    //
    // Normal: require Ncdf(r) - Ncdf(l) >= .05
    // Halfnormal: Obviously required half-sidedness; also require Ncdf(r) - Ncdf(l) >= .025
    // Uniform: if (l < 0 < r) require Npdf(l) / Npdf(0) >= 0.05 and likewise for r.
    //          otherwise (one-sided) require Npdf(outer) / Npdf(inner) >= 0.05
    // Exponential: Obviously required half-sidedness; also require Npdf(outer) < 0.75 Npdf(inner)

    constexpr double mu = approx_zero, sigma = approx_one;

    boost::math::normal_distribution<double> N01(mu, sigma);

    auto &rng = eris::random::rng();

    std::cout << "left,right,normal,halfnormal,uniform,exponential\n";
    while (true) {
        double l = draw_random_parameter(), r = draw_random_parameter();
        // If we drew identical parameters (probably both - or both + infinity, which has about a
        // 1/50 chance of occuring; it could also, technically, be the drawn values, though the
        // probability of that is negligible).
        if (l == r) continue;
        if (l > r) std::swap(l, r);

        benchmark b(l, r);
        // Normal:
        if (cdf(N01, r) - cdf(N01, l) > 0.05) b.normal = true;
        // Halfnormal:
        if ((r <= mu or mu <= l) and cdf(N01, r) - cdf(N01, l) > 0.025) b.halfnormal = true;
        // Uniform: straddling 0:
        if (l < mu and mu < r) {
            if (std::min(pdf(N01, l), pdf(N01, r)) >= 0.05 * boost::math::constants::one_div_root_two_pi<double>()) b.uniform = true;
        }
        // Uniform: right tail:
        else if (l >= mu) { if (pdf(N01, r) >= 0.05 * pdf(N01, l)) b.uniform = true; }
        // Uniform: left tail:
        else { if (pdf(N01, l) >= 0.05 * pdf(N01, r)) b.uniform = true; }
        // Exponential, right-tail:
        if (l >= mu) { if (pdf(N01, r) < 0.75 * pdf(N01, l)) b.exponential = true; }
        // Exponential, left-tail:
        else if (r <= mu) { if (pdf(N01, l) < 0.75 * pdf(N01, r)) b.exponential = true; }

        std::ostringstream csv;
        csv.precision(17);
        csv << b.left << "," << b.right;
        if (b.normal) {
            b.timing.normal = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, b.left, b.right);
                    });
            csv << "," << b.timing.normal.first / b.timing.normal.second;
        }
        else csv << ",nan";

        if (b.halfnormal) {
            b.timing.halfnormal = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, sigma, b.left, b.right, b.left >= mu);
                    });
            csv << "," << b.timing.halfnormal.first / b.timing.halfnormal.second;
        }
        else csv << ",nan";

        if (b.uniform) {
            const double inv2s2 = 0.5 / (sigma*sigma);
            const double shift2 = b.left >= mu ? (b.left - mu)*(b.left - mu) : b.right <= mu ? (b.right - mu)*(b.right - mu) : 0;
            b.timing.uniform = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_uniform(rng, mu, b.left, b.right, inv2s2, shift2);
                    });
            csv << "," << b.timing.uniform.first / b.timing.uniform.second;
        }
        else csv << ",nan";

        if (b.exponential) {
            const double bound_dist = b.left >= mu ? (b.left - mu) : (mu - b.right);
            const double proposal_param = 0.5 * (bound_dist + sqrt(bound_dist*bound_dist + 4*sigma*sigma));
            b.timing.exponential = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, b.left, b.right, b.left >= mu, bound_dist, proposal_param);
                    });
            csv << "," << b.timing.exponential.first / b.timing.exponential.second;
        }
        else csv << ",nan";

        csv << "\n";

        std::cout << csv.str() << std::flush;
    }

    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";

}
