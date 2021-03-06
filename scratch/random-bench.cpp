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
    bool normal{false}, halfnormal{false}, uniform{false}, exponential{false};
    struct {
        std::pair<long, double> selected, normal, halfnormal, uniform, exponential, expo_approx;
    } timing;
    benchmark(double l, double r)
        : left{l}, right{r}
    {}
};

double draw_random_parameter() {
    double d =
        boost::random::uniform_01<double>()(eris::random::rng()) < 0.2
        ? std::numeric_limits<double>::infinity()
        : eris::random::exponential_distribution<double>(0.5)(eris::random::rng());

    if (boost::random::uniform_01<double>()(eris::random::rng()) < 0.5)
        d = -d;

    return d;
}

const std::vector<double> left_2s_values{{-10.,-5.,-3.,-2.,-1.,-.5,-.4,-.3,-.2,-.1,0.,.1,.2,.3,.4,.5,1.,2.,3.,5.,10.}};

double draw_random_2s_left() {
    std::uniform_int_distribution<int> random_i(0, left_2s_values.size()-1);
    return left_2s_values[random_i(eris::random::rng())];
}

// Draws a right value for the given left value.  If range is nan, draws Exp(0.5) and adds it to
// left; otherwise, draws U[0, range] and adds it to left.
double draw_random_2s_right(double left, double range) {
    auto &rng = eris::random::rng();
    if (std::isnan(range)) {
        return left + eris::random::exponential_distribution<double>(0.5)(rng);
    }
    else {
        return boost::random::uniform_real_distribution<double>(left, left + range)(rng);
    }
}

std::string double_str(double d, unsigned precision = std::numeric_limits<double>::max_digits10) {
    double round_trip;
    for (unsigned prec : {precision-2, precision-1}) {
        std::stringstream ss;
        ss.precision(prec);
        ss << d;
        ss >> round_trip;
        if (round_trip == d) { ss << d; return ss.str(); }
    }
    std::stringstream ss;
    ss.precision(precision);
    ss << d;
    return ss.str();
}



// Accumulate results here, so that they can't be compiled away:
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
    double custom_left = std::numeric_limits<double>::quiet_NaN();
    double two_range = std::numeric_limits<double>::quiet_NaN();
    // Match floats, but don't allow things like nan or inf:
    std::regex numre(R"([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)");
    if (argc == 2) {
        runmode = argv[1];
        if (runmode == "RANDOM" or runmode == "LEFT" or runmode == "RIGHT" or runmode == "TWO") {
            bad_args = false;
        }
        else if (std::regex_match(runmode, numre)) {
            bad_args = false;
            custom_left = std::stod(runmode);
            runmode = "CUSTOML";
        }
        else {
            std::cerr << "Invalid/unknown argument `" << runmode << "'\n\n";
        }
    }
    else if (argc == 3) {
        if (std::regex_match(argv[1], numre) and std::regex_match(argv[2], numre)) {
            custom_left = std::stod(argv[1]);
            two_range = std::stod(argv[2]) - custom_left;
            if (two_range <= 0) {
                std::cerr << "Invalid v1 v2 values: v2 > v1 required\n\n";
            }
            else {
                runmode = "CUSTOML";
                bad_args = false;
            }
        }
        else std::cerr << "Invalid/unknown arguments `" << argv[1] << "' `" << argv[2] << "'\n\n";
    }
    else {
        std::cerr << "Invalid/unknown arguments\n\n";
    }

    if (bad_args) {
        std::cerr << "Usage: " << argv[0] << " {RANDOM|LEFT|RIGHT|TWO|value|value value}\n\n";
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
                     "TWO    - draw left from ±{10,-5,-3,-2,-1,-0.5,-0.4,...,-0.1,0,0.1,...,0.4,0.5,1,2,3,5,10},\n"
                     "         draw right from left + Exp(0.5)\n\n" <<
                     "value  - like TWO, but use <value> for the left limit.\n\n" <<
                     "v1 v2  - like TWO, but use <v1> for the left limit and draw right from Unif[v1,v2]\n\n" <<
                     std::flush;
        return 1;
    }

    constexpr double mu = approx_zero, sigma = approx_one;
    volatile double sigma_v = sigma;


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
    // Uniform, straddling 0: enable if r-l less than this:
    constexpr double MAX_UNIFORM_RANGE_INCL_ZERO = 20.;
    // Uniform, one-tail: if |inner| < 2, then enable if |outer| - |inner| less than this:
    constexpr double MAX_UNIFORM_RANGE_INNER_TAIL = 10.;
    // Uniform, one-tail: if |inner| >= 2, then enable if |outer| - |inner| < this / |inner|:
    // (so at left=2, enable for right < 12; at left=100, enable for right < 100.2).
    constexpr double MAX_UNIFORM_RANGE_OUTER_TAIL_RATIO = 20.;
    // Exponential: Obviously required half-sidedness; also require Npdf(outer) < Npdf(inner) times:
    constexpr double MAX_EXPONENTIAL_PDF_RATIO = 0.9;

    boost::math::normal_distribution<double> N01(mu, sigma);

    auto &rng = eris::random::rng();


    // Warm-up (let CPU get to max speed) for at least 2 seconds:
    bench([&]() -> double { return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, -1.0, 1.0); },
            2.0);

    // The first two values are the left/right values; then the speeds (in draws/second) for the
    // various methods.  expo_approx uses the lambda = a approximation; exponential uses the
    // optimal lambda value (including the calculation in the benchmarked code).
    std::cout << "left,right,selected,normal,halfnormal,uniform,exponential,expo_approx\n";
    for (int i = 0; i < 10000; i++) {
        double l =
            (runmode == "RANDOM" or runmode == "LEFT")  ? draw_random_parameter() :
            (runmode == "TWO") ? draw_random_2s_left() :
            (runmode == "CUSTOML") ? custom_left :
            -std::numeric_limits<double>::infinity();
        double r = (runmode == "RANDOM" or runmode == "RIGHT") ? draw_random_parameter() :
            (runmode == "TWO" || runmode == "CUSTOML") ? draw_random_2s_right(l, two_range) :
            std::numeric_limits<double>::infinity();

        // If we drew identical parameters (probably both - or both + infinity, which has about a
        // 1/50 chance of occuring (under RANDOM); it could also, technically, be the drawn values,
        // though the probability of that is negligible).
        if (l == r) { i--; continue; }
        if (l > r) std::swap(l, r);

        benchmark b(l, r);
        // Normal:
        if (cdf(N01, r) - cdf(N01, l) >= MIN_NORMAL_PROB) b.normal = true;
        // Halfnormal:
        if ((r <= mu or mu <= l) and cdf(N01, r) - cdf(N01, l) > 0.5*MIN_NORMAL_PROB) b.halfnormal = true;
        // Uniform: straddling 0:
        if (l <= mu and mu <= r) {
            b.uniform = (r - l < MAX_UNIFORM_RANGE_INCL_ZERO);
        }
        // Uniform, in one tail but starting not too far out:
        else if (l > mu ? l < mu + 2.0 : r > mu - 2.0) {
            b.uniform = (r - l < MAX_UNIFORM_RANGE_INNER_TAIL);
        }
        // Uniform, in one tail, starting far out in the tail:
        else {
            b.uniform = (r - l < MAX_UNIFORM_RANGE_OUTER_TAIL_RATIO / std::fabs(r < mu ? r : l));
        }
        // Exponential, right-tail:
        if (l > mu and l > 0) { if (pdf(N01, r) < MAX_EXPONENTIAL_PDF_RATIO * pdf(N01, l)) b.exponential = true; }
        // Exponential, left-tail:
        else if (r < mu and r < 0) { if (pdf(N01, l) < MAX_EXPONENTIAL_PDF_RATIO * pdf(N01, r)) b.exponential = true; }

        std::ostringstream csv;
        csv << double_str(b.left) << "," << double_str(b.right);

        b.timing.selected = bench([&]() -> double {
               return eris::random::truncated_normal_distribution<double>(mu, sigma, b.left, b.right)(rng);
               });
        csv << "," << double_str(b.timing.selected.first / b.timing.selected.second);

        if (b.normal) {
            b.timing.normal = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_normal(rng, mu, sigma, b.left, b.right);
                    });
            csv << "," << double_str(b.timing.normal.first / b.timing.normal.second);
        }
        else csv << ",nan";

        if (b.halfnormal) {
            b.timing.halfnormal = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_halfnormal(rng, mu, b.left >= mu ? sigma : -sigma, b.left, b.right);
                    });
            csv << "," << double_str(b.timing.halfnormal.first / b.timing.halfnormal.second);
        }
        else csv << ",nan";

        if (b.uniform) {
            const double inv2s2 = 0.5 / (sigma*sigma);
            const double shift2 = b.left >= mu ? (b.left - mu)*(b.left - mu) : b.right <= mu ? (b.right - mu)*(b.right - mu) : 0;
            b.timing.uniform = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_uniform(rng, mu, b.left, b.right, inv2s2, shift2);
                    });
            csv << "," << double_str(b.timing.uniform.first / b.timing.uniform.second);
        }
        else csv << ",nan";

        if (b.exponential) {
            const double bound_dist = b.left >= mu ? (b.left - mu) : (mu - b.right);
            volatile double bound_dist_v = bound_dist;
            b.timing.exponential = bench([&]() -> double {
                    double proposal_param = 0.5 * (bound_dist_v + sqrt(bound_dist_v*bound_dist_v + 4*sigma_v*sigma_v));
                    return eris::random::detail::truncnorm_rejection_exponential(rng, mu, sigma, b.left, b.right, bound_dist, proposal_param);
                    });
            csv << "," << double_str(b.timing.exponential.first / b.timing.exponential.second);

            b.timing.expo_approx = bench([&]() -> double {
                    return eris::random::detail::truncnorm_rejection_exponential(rng, mu, sigma, b.left, b.right, bound_dist, bound_dist);
                    });
            csv << "," << double_str(b.timing.expo_approx.first / b.timing.expo_approx.second);

        }
        else csv << ",nan,nan";

        csv << "\n";

        std::cout << csv.str() << std::flush;
    }

    if (garbage == 1.75) std::cout << "# Garbage = 1.75 -- this is almost impossible\n";

}
