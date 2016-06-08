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

struct benchmark {
    const double left, right;
    bool normal{false}, halfnormal{false}, uniform{false}, exponential{false}, expo_approx{false};
    struct {
        std::pair<long, double> normal, halfnormal, uniform, exponential, expo_approx;
    } timing;
    benchmark(double l, double r)
        : left{l}, right{r}
    {}
};

double draw_random_parameter() {
    double d =
        boost::random::uniform_01<double>()(eris::random::rng()) < 0.2
        ? std::numeric_limits<double>::infinity()
        : std::exponential_distribution<double>(0.5)(eris::random::rng());

    if (boost::random::uniform_01<double>()(eris::random::rng()) < 0.5)
        d = -d;

    return d;
}

// We store results here, so that they can't be compiled away.
double garbage = 0.0;
// Runs code for at least the given time; returns the number of times run, and the total runtime.
std::pair<long, double> bench(std::function<double()> f, double at_least = 0.25) {
    auto start = clk::now();
    std::pair<long, double> results;
    int increment = 50;
    do {
        increment *= 2;
        for (int i = 0; i < increment; i++) {
            garbage += f();
        }
        results.second = dur(clk::now() - start).count();
        results.first += increment;
    } while (results.second < at_least);

    return results;
}

int main(int argc, char *argv[]) {

    bool bad_args = true;
    std::string runmode;
    if (argc == 2) {
        runmode = argv[1];
        if (runmode == "RANDOM" or runmode == "LEFT" or runmode == "RIGHT") {
            bad_args = false;
        }
    }

    if (bad_args) {
        std::cerr << "Usage: " << argv[0] << " {RANDOM|LEFT|RIGHT}\n\n";
        std::cerr << "Run modes:\n\n";
        std::cerr << "RANDOM - randomly draw left/right truncation points from:\n" <<
                     "         ⎧ +∞ with prob. 0.1\n" <<
                     "         ⎨ -∞ with prob. 0.1\n" <<
                     "         ⎪ -Exp(0.5) with prob. 0.4\n" <<
                     "         ⎩ +Exp(0.5) with prob. 0.4\n" <<
                     "\n" <<
                     "         left/right values are swapped if right < left\n"
                     "         left/right are redrawn if left == right\n"
                     "\n\n" <<
                     "LEFT   - left truncation point drawn as above, right = +∞\n\n" <<
                     "RIGHT  - right truncation point drawn as above, left = -∞\n\n" <<
                     std::flush;
        return 1;
    }

    constexpr double mu = approx_zero, sigma = approx_one;


    // RANDOM: draw both truncation points randomly.  We do this by drawing infinity with
    // probability 0.1, and otherwise take a draw from exp(.5).  This is then multiplied by a random
    // -1/+1 modifier.
    //
    // LEFT: set right = +infinity, draw left as above
    // RIGHT: set left = -infinity, draw right as above
    //
    // For pair of parameters so obtained, we figure out which rejection methods are suitable
    // according to:
    //
    // Normal: require Ncdf(r) - Ncdf(l) >=
    constexpr double MIN_NORMAL_PROB = .01;
    // Halfnormal: Obviously required half-sidedness; also require Ncdf(r) - Ncdf(l) >= half of above
    // Uniform: if (l < 0 < r) require Npdf(l) / Npdf(0) >=
    constexpr double MIN_UNIFORM_PDF_RATIO = 0.01; // and likewise for r
    //          otherwise (one-sided) require Npdf(outer) / Npdf(inner) >= same value
    // Exponential: Obviously required half-sidedness; also require Npdf(outer) < Npdf(inner) times:
    constexpr double MAX_EXPONENTIAL_PDF_RATIO = 0.9;

    boost::math::normal_distribution<double> N01(mu, sigma);

    auto &rng = eris::random::rng();


    // Warm-up (let CPU get to max speed) for at least 2 seconds:
    bench([&]() -> double { return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, -1.0, 1.0); },
            2.0);

    std::cout << "left,right,normal,halfnormal,uniform,exponential,expo_approx\n";
    while (true) {
        double l = (runmode == "RANDOM" or runmode == "LEFT")  ? draw_random_parameter() : -std::numeric_limits<double>::infinity();
        double r = (runmode == "RANDOM" or runmode == "RIGHT") ? draw_random_parameter() :  std::numeric_limits<double>::infinity();

        // If we drew identical parameters (probably both - or both + infinity, which has about a
        // 1/50 chance of occuring (under RANDOM); it could also, technically, be the drawn values,
        // though the probability of that is negligible).
        if (l == r) continue;
        if (l > r) std::swap(l, r);

        benchmark b(l, r);
        // Normal:
        if (cdf(N01, r) - cdf(N01, l) >= MIN_NORMAL_PROB) b.normal = true;
        // Halfnormal:
        if ((r <= mu or mu <= l) and cdf(N01, r) - cdf(N01, l) > 0.5*MIN_NORMAL_PROB) b.halfnormal = true;
        // Uniform: straddling 0:
        if (l < mu and mu < r) {
            if (std::min(pdf(N01, l), pdf(N01, r)) >= MIN_UNIFORM_PDF_RATIO * boost::math::constants::one_div_root_two_pi<double>()) b.uniform = true;
        }
        // Uniform: right tail:
        else if (l >= mu) { if (pdf(N01, r) >= MIN_UNIFORM_PDF_RATIO * pdf(N01, l)) b.uniform = true; }
        // Uniform: left tail:
        else { if (pdf(N01, l) >= MIN_UNIFORM_PDF_RATIO * pdf(N01, r)) b.uniform = true; }
        // Exponential, right-tail:
        if (l > mu) { if (pdf(N01, r) < MAX_EXPONENTIAL_PDF_RATIO * pdf(N01, l)) b.exponential = true; b.expo_approx = true; }
        // Exponential, left-tail:
        else if (r < mu) { if (pdf(N01, l) < MAX_EXPONENTIAL_PDF_RATIO * pdf(N01, r)) b.exponential = true; b.expo_approx = true; }

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

        if (b.expo_approx) {
            const double bound_dist = b.left >= mu ? (b.left - mu) : (mu - b.right);
            b.timing.expo_approx = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_exponential(rng, sigma, b.left, b.right, b.left >= mu, bound_dist, bound_dist);
                    });
            csv << "," << b.timing.expo_approx.first / b.timing.expo_approx.second;
        }
        else csv << ",nan";

        csv << "\n";

        std::cout << csv.str() << std::flush;
    }

    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";

}
