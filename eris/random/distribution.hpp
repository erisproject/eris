#pragma once
#include <eris/random/rng.hpp>
#include <random>
#include <stdexcept>

namespace eris { namespace random {

/** A pre-defined, per-thread, reusable N(0,1) distribution.  Using this is preferred to using your
 * own when requesting only a single value as normal values are typically generated in pairs (thus
 * using this should be around twice as fast as constructing multiple std::normal_distribution
 * objects that are used only once).
 *
 * \sa rstdnorm()
 */
thread_local extern std::normal_distribution<double> stdnorm;

/** Returns a draw from a double N(0,1) distribution using the current rng().  Equivalent to:
 * `eris::random::stdnorm(eris::random::rng())`.  If drawing multiple values at once, it is
 * preferrable to use stdnorm directly, or to use your own std::normal_distribution<double> object,
 * in either case using a stored rng() reference (which avoids the slight overhead of needing to
 * make sure the RNG is seeded each time).
 */
inline double rstdnorm() {
    return stdnorm(rng());
}


/** Returns a draw from a truncated univariate distribution given the truncation points.  The
 * complements are used to ensure better numerical precision for regions of the distribution with
 * cdf values above 0.5.
 *
 * If the given min and max values are at (or beyond) the minimum and maximum limits of the
 * distribution, a simple draw (without truncation) is returned from the distribution.  Otherwise,
 * if the truncation region and distribution supports imply a truncation region of a single point
 * (for example, a distribution with support \f$[0,1]\f$ truncated to \f$[1,\infty)\f$), the single
 * point is returned.
 *
 * Otherwise, the cdf values of min and max are calculated, then a U[cdf(min), cdf(max)] is drawn
 * and the distribution quantile at that draw is returned.  (Note, however, that the algorithm also
 * uses cdf complements when using a cdf would result in a loss of numerical precision).
 *
 * Note that when calculting a cdf and/or quantile is very expensive for a distribution, it is often
 * beneficial to use rejection sampling instead, particularly when the truncation region covers a
 * relatively large portion of the underlying distribution.  This function provides two such
 * mechanisms: see the `precdf_draws` and `invcdf_below` parameters.
 *
 * \param dist any floating point (typically double) distribution object which supports `cdf(dist,
 * x)`, `quantile(dist, p)`, `cdf(complement(dist, x))`, and `quantile(complement(dist, q))` calls,
 * and has a `value_type` member indicating the type of value handled by the distribution.  The
 * distribution objects supported by boost (such as `boost::math::normal_distribution` are
 * intentionally suitable.
 *
 * \param generator a random number generator such that `generator(eris::Random::rng())` return a
 * random draw from the untruncated distribution, and has a `result_type` member agreeing with
 * `dist::value_type`.  This generator is used when the given min and max don't actually truncate
 * the distribution, and for random sampling draws when `precdf_draws` is positive or when the cdf
 * range is sufficiently large to prefer rejection sampling according to the `invcdf_below`
 * parameter.  Both boost's random number generators and the C++11 random number generators are
 * suitable.
 *
 * \param min the truncated region lower bound
 *
 * \param max the truncated region upper bound
 *
 * \param median the median of the distribution, above which cdf complements will be used and below
 * which cdf values will be used.  When the median is not extremely simple (for example, a normal
 * distribution with median=mean), omit this (or specify it as NaN): in the worst case (which occurs
 * whenever both min and max are on the same side of the median), 3 cdf calls will be used (either
 * two cdfs and one cdf complement, or one cdf and two complements).  If specified, exactly two cdf
 * calls are used always.
 *
 * \param invcdf_below specifies the cdf range below which the algorithm will draw by passing a
 * random uniform value through the inverse cdf.  If the cdf range is above this value, this
 * algorithm will use rejection sampling instead.  Warning: setting this to a very small value can
 * result in a truncDist call drawing a very large number of values before returning.  The optimal
 * value for this really depends on the distribution, and in particular, how quickly quantiles can
 * be calculated from probabilities.  The default, 0.3, seems roughly appropriate for a normal
 * distribution.  A chi squared distribution, which has expensive inverse cdf lookups, seems to have
 * an optimal value of around 0.05.
 *
 * \param precdf_draws specifies that number of draws from the distribution that will be attempted
 * before calculating cdf values.  When drawing random values is cheap compared to calculating cdfs,
 * specifying this value can be very beneficial whenever the truncation range isn't too small.  The
 * default is 0 (i.e. don't do pre-cdf calculation draws), which is suitable for a normal
 * distribution or other distribution that has a relatively simple closed-form cdf calculation.
 * Setting this to a positive value can make a substantial difference for other distributions (such
 * as chi-squared), when the truncation range isn't in an extremely unlikely part of the
 * distribution support.
 *
 * \returns a draw (of type `dist::value_type`) from the truncated distribution.
 *
 * \throws std::runtime_error subclass (currently either std::range_error or std::underflow_error,
 * see below) if a value cannot be drawn.
 * \throws std::range_error if called with min > max or a truncation region entirely outside the
 * support of the distribution.
 * \throws std::underflow_error if the truncation range is so far in the tail that both `min` and
 * `max` cdf values (or both cdf complement values) are 0 or closer to 0 than a double value can
 * represent without loss of numerical precision (approximately 2.2e-308 for a double, though
* doubles can store subnormal values as small as 4.9e-324 with reduced numerical precision).
 */
template <class DistType, class RNGType, typename ResultType = typename DistType::value_type>
#ifdef DOXYGEN_SHOULD_SEE_THIS
ResultType truncDist(
#else
static auto truncDist(
#endif
    const DistType &dist,
    RNGType &generator,
    ResultType min,
    ResultType max,
    ResultType median = std::numeric_limits<ResultType>::signaling_NaN(),
    double invcdf_below = 0.3,
    unsigned int precdf_draws = 0
    )
#ifndef DOXYGEN_SHOULD_SEE_THIS
    -> typename std::enable_if<
            // Check that DistType and RNGType operate on the same type of floating-point variable:
            std::is_same<typename DistType::value_type, typename RNGType::result_type>::value and
            std::is_floating_point<typename DistType::value_type>::value and
            // Check that the required cdf/quantile/complement ADL functions are callable
            std::is_same<ResultType, decltype(cdf(dist, ResultType{}))>::value and
            std::is_same<ResultType, decltype(cdf(complement(dist, ResultType{})))>::value and
            std::is_same<ResultType, decltype(quantile(dist, ResultType{}))>::value and
            std::is_same<ResultType, decltype(quantile(complement(dist, ResultType{})))>::value,
            ResultType
        >::type
#endif
{
    if (min > max) throw std::range_error("truncDist() called with empty truncation range (min > max)");

    auto dist_range = range(dist);
    auto &dist_min = dist_range.first, &dist_max = dist_range.second;
    if (min <= dist_min and max >= dist_max)
        // Truncation range isn't limiting the distribution, so just return an ordinary draw
        return generator(rng());
    if (max < dist_min or min > dist_max)
        throw std::range_error("truncDist() called with empty effective truncation range ([min,max] outside distribution support)");
    if (max == min or max == dist_min)
        // If max equals min (or dist_min), the truncation range is a single value, so just return it
        return max;
    if (min == dist_max)
        // Likewise for min equaling dist_max
        return min;

    for (unsigned int i = 0; i < precdf_draws; i++) {
        double x = generator(rng());
        if (x >= min and x <= max) return x;
    }

    ResultType alpha, omega;
    bool alpha_comp, omega_comp;
    if (std::isnan(median)) { // No median, so use an extra call to figure out if we need complements
        alpha = min <= dist_min ? 0.0 : cdf(dist, min);
        alpha_comp = alpha > 0.5;
        if (alpha_comp) {
            // We should have taken the complement but didn't, so get cdf again, but this time as cdf complement
            alpha = cdf(complement(dist, min));
        }

        omega = max >= dist_max ? 0.0 /*==complement of 1*/ : cdf(complement(dist, max));
        omega_comp = alpha_comp || omega < 0.5; // If alpha needed a complement, omega certainly
        // will; otherwise we're guessing that we'll want the complement

        if (not omega_comp) {
            // We guessed wrong: omega is actually in the left tail, so don't want the complement
            omega = cdf(dist, max);
        }
    }
    else {
        if (min <= dist_min) { alpha_comp = false; alpha = 0.0; }
        else {
            alpha_comp = min > median;
            alpha = alpha_comp ? cdf(complement(dist, min)) : cdf(dist, min);
        }
        if (max >= dist_max) { omega_comp = true; omega = 0.0; }
        else {
            omega_comp = max > median;
            omega = omega_comp ? cdf(complement(dist, max)) : cdf(dist, max);
        }
    }

    if (not alpha_comp and omega_comp) {
        // min is left of the median and max is right: we need either make both complements or both
        // non-complements.  The most precision is going to be lost by whichever value is smaller,
        // so complement the larger value (so we'll either get both complements, or both
        // non-complements).
        if (alpha > omega) { alpha = 1 - alpha; alpha_comp = true; }
        else { omega = 1 - omega; omega_comp = false; }
    }
    // (alpha_comp and not omega_comp) is impossible, because that would mean 'min > max'
    // and we would have thrown an exception above.

    if (std::fabs(omega - alpha) >= invcdf_below) {
        double x;
        do {
            x = generator(rng());
        } while (x < min or x > max);
        return x;
    }
    else {
        if (alpha_comp) {
            // Check for underflow (essentially: is 1-alpha equal to or closer to 0 than a ResultType
            // (typically a double) can represent without reduced precision)
            if (alpha == 0 or std::fpclassify(alpha) == FP_SUBNORMAL)
                throw std::underflow_error("truncDist(): Unable to draw from truncated distribution: truncation range is too far in the upper tail");

            // Both alpha and omega are complements, so take a draw from [omega,alpha], then pass it
            // through the quantile_complement
            return quantile(complement(dist, std::uniform_real_distribution<ResultType>(omega, alpha)(rng())));
        }
        else {
            // Check for underflow (essentially, is omega equal to or closer to 0 than a ResultType
            // (typically a double) can represent without reduced precision)
            if (omega == 0 or std::fpclassify(omega) == FP_SUBNORMAL)
                throw std::underflow_error("truncDist(): Unable to draw from truncated distribution: truncation range is too far in the lower tail");

            // Otherwise they are ordinary cdf values, draw from the uniform and invert:
            return quantile(dist, std::uniform_real_distribution<ResultType>(alpha, omega)(rng()));
        }
    }
}

// FIXME: add a boost::distribution::normal specialization that does Geweke's implementation

}}
