#pragma once
#include <limits>
#include <boost/random/detail/operators.hpp>
#include <eris/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/uniform_01.hpp>
#include <eris/random/exponential_distribution.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/core/scoped_enum.hpp>

namespace eris { namespace random {

namespace detail {

// Constants for the truncated normal algorithm.  All of these values depend on the specific CPU,
// architecture, compiler, etc., so we only try to provide ballpark figures here that should work
// reasonably well.
template<class RealType>
struct truncnorm_threshold {
    /** The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use exponential
     * rejection sampling instead of half normal rejection sampling.  This value applies when the
     * truncation range is contained entirely within one side of the distribution.  (Note, however,
     * that on either side of this condition, we may still resort to uniform rejection sampling if b
     * is too close to a).
     */
    static constexpr RealType hr_below_er_above = RealType(0.55);

    /** The value of `a*(b-a)` below which to prefer UR to ER when considering 2-sided truncation
     * draws in the tails of the distribution.
     *
     * In the tails of the distribution the truncated normal is very similar to the exponential: the
     * rescaled conditional distribution looks essentially identical.  This in turn means that we
     * can use a very simple rule for deciding between ER and UR for tail rejection sampling: if
     * `b-a < c/a` we want to prefer UR, otherwise ER.  Essentially what this means is that the
     * threshold value of the truncation range is inversely proportional to the left edge of the
     * truncation.
     *
     * We actually use this all the time when deciding between ER and UR, i.e. all the way down to
     * hr_below_er_above: the value is a little bit off for lower values (it should be around 0.34
     * at a=0.55, and around 0.325 at a=0.75, and 0.32 at a=1, but this are all pretty close
     * performance-wise (less than 3% off for 0.55).
     */
    static constexpr RealType prefer_ur_multiplier = 0.31;

    /** This is the threshold b-a value for deciding between NR and UR: when b-a is above this,
     * rejection sampling from a normal distribution is preferred; below this, uniform rejection
     * sampling is preferred.  This value is only used when the truncation range includes values on
     * both sides of the mean.
     *
     * This value equals sqrt(2*pi) times the ratio of normal draw cost to uniform sampling iteration
     * cost (remember that a uniform sample iteration typically requires drawing 2 uniforms and
     * evaluating one exponential, while normal rejection simply involves drawing a normal).
     */
    static constexpr RealType ur_below_nr_above = RealType(1);

    /** This is the average cost of a single halfnormal draw, relative to the cost of a single
     * uniform accept/reject iteration.
     */
    static constexpr RealType cost_hr_rel_ur = RealType(0.275);
};

/** The Taylor expansion order to use when approximating e^x when deciding between UR and HR.  The
 * minimum required approximation order depends on the specific value of `hr_below_er_above`
 * and the speed of performing the approximation calculations; it is typically 1 or 2, but 2 or 3
 * are often a better choice.
 *
 * This is currently a 3rd-order Taylor expansion of e^x, evaluated at 0 (and so, technically, a
 * 3rd-order Maclaurin series expansion of e^x), because in practice a 3rd order expansion is almost
 * always sufficient, and seems to have neglible computation cost over a 3nd order expansion.
 *
 * draw-perf reports a few things in this regards: first, it reports whether a T1, T2, or higher
 * order approximation is required for using the approximation to be at least as good as calculating
 * the precise optimal value; the order of this implementation should be no smaller than that.  For
 * example, in my testing, for boost and gsl-zigg I get a value of 2, for the others a value of 1.
 *
 * Secondly, draw-pref reports the values of a at which a Tn approximation is preferred to a
 * lower-order approximation--usually T{n-1}, but sometimes T{n-2}, depending on CPU/compiler
 * optimizations.  For example (note that these values change slightly from run-to-run, and change
 * more noticeably across compilers and CPUs), one run of draw-perf resulted in T2 being preferred
 * above a=0.4987, but T3 being preferred above a=0.4791--in other words, T3 is always better than
 * T2, and better than T1 for a > 0.4791.  T4 is preferred for a > 1.1656, but this is close enough
 * to the reported a0 (1.3619) that it probably isn't worth doing, since it will be less efficient
 * for values below the 1.1656 threshold.  So, since we need at least T2 to cover the [0,a0]
 * potential range, and since T3 is apparently always better than T2, we use T3.
 *
 * It is, in theory, possible to use multiple approximations depending on the value of \f$\alpha\f$
 * (continuing the above example, using T1 for [0,0.4791], T3 for (0.4791,1.1656], T4 for
 * (1.1656,1.3619]), but in practice the overhead of checking and branching isn't worthwhile
 * (especially when the highest we should use, according to draw-perf, is T4).
 *
 * Another possibility is to consider a Taylor expansion around another point--say, a0^2/4 (because
 * we evaluate this approximation at a^2/2 for a between 0 and a0)--but this ends up trading away
 * some accurancy near 0 for some accuracy near a0, and is only slightly more accurate than T3
 * around a0, and much worse around 0; better, then, to use use T3.  (The story between T3@a and
 * T4@0 is similar).  So again, not worth the effort.
 *
 * Currently supported values are from 0 to 5.
 */
#define ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER 3

/** The Taylor series expansion (actually, the Maclaurin series expansion, which is a Taylor
 * expansion around 0) of e^x.
 */
template<class RealType>
inline RealType exp_maclaurin(const RealType &x) {
    return RealType(1)
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 1
        + x * (RealType(1)
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 2
        + x * (RealType(1)/2
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 3
        + x * (RealType(1)/6
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 4
        + x * (RealType(1)/24
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 5
        + x * (RealType(1)/120
#if ERIS_RANDOM_DETAIL_TRUNCATED_NORMAL_DISTRIBUTION_TAYLOR_ORDER >= 6
#error Taylor orders above 5 are not implemented
// If ever implementing this, note that above 5, it seems (at least with basic SSE optimizations) to
// be faster to compute the expansion as:
//     1 + x + x*x*(1/2 + 1/6*x + x*x*(1/24 + ...
// instead of the alternative, used above, which is faster below 5:
//     1 + x*(1 + x*(1/2 + ...
//
#endif
        )
#endif
        )
#endif
        )
#endif
        )
#endif
        )
#endif
        ;
}

/** Perform rejection-sampling from a normal distribution, rejecting any draws outside the range
 * `[lower, upper]`.  This is naive rejection sampling: we simply keep drawing from the untruncated
 * distribution until we get one in the desired range.
 */
template <class Engine, class RealType, class Normal = eris::random::normal_distribution<RealType>>
RealType truncnorm_rejection_normal(Engine &eng, const RealType &mu, const RealType &sigma, const RealType &lower, const RealType &upper) {
    RealType x;
    Normal normal(mu, sigma);
    do { x = normal(eng); } while (x < lower or x > upper);
    return x;
}

/** Perform rejection sampling from a half-normal distribution, rejecting any draws outside the
 * range `[lower, upper]`.  This obviously only makes sense when `lower` and `upper` are on the same
 * side of `mu`.
 *
 * \param eng the random engine to use to obtain random values
 * \param mu the distribution parameter of the halfnormal distribution.  For an untruncated, normal
 * (not halfnormal) distribution, this would be the distribution mean; for a halfnormal
 * distribution, this is also one of the support boundaries of the (untruncated) halfnormal
 * distribution.
 * \param sigma the distribution parameter of the halfnormal distribution (for an untruncated, full
 * normal distribution, this is the standard deviation).
 * \param lower the lower truncation point
 * \param upper the upper truncation point
 * \param positive if true, draw (before considering lower/upper rejection) from a positive
 * halfnormal distribution (i.e. with support [mu, infinity); if false, draw from a negative
 * halfnormal distribution (i.e. with support (-infinity, mu]).
 */
template <class Engine, class RealType, class Normal = eris::random::normal_distribution<RealType>>
RealType truncnorm_rejection_halfnormal(Engine &eng, const RealType &mu, const RealType &sigma, const RealType &lower, const RealType &upper, bool right_tail) {
    const RealType signed_sigma(right_tail ? sigma : -sigma);
    RealType x;
    Normal normal;
    using std::fabs;
    do {
        x = mu + signed_sigma * fabs(normal(eng));
    } while (x < lower or x > upper);
    return x;
}

/** Perform rejection sampling using a uniform distribution.
 *
 * This procedure operates by drawing a uniform value in `[lower, upper)` and rejecting it
 * probabilistically with probability of the ratio of the normal density to the uniform density at
 * the drawn value, thus resulting in the appropriate (truncated) normal distribution.  This is
 * guaranteed to perform poorly when the range is large; it is mainly designed to be used when the
 * truncation range is very small, so that the probabilistic rejection is relatively low.
 *
 * \param eng the random engine to use to obtain random values
 * \param mu the mean parameter of the pre-truncation normal distribution.
 * \param lower the lower bound of the truncated range (should be finite)
 * \param upper the upper bound of the truncated range (should be finite)
 * \param inv2s2 the pre-computed value of 0.5 / sigma^2.  Since this is often precomputed when
 * deciding which method to use, it is more efficient to preserve the pre-computed value.
 * \param shift2 the pre-computed `shift^2` parameter: if `[lower,upper]` includes 0, this should be
 * 0; otherwise it should be the squared value of the difference between the closer-to-mu boundary
 * and mu.  As with `inv2s2`, the pre-computed value is used for efficiency.
 */
template <class Engine, class RealType,
         class Uniform = boost::random::uniform_real_distribution<RealType>,
         class Unif01 = boost::random::uniform_01<RealType>>
RealType truncnorm_rejection_uniform(Engine &eng, const RealType &mu, const RealType &lower, const RealType &upper,
        const RealType &inv2s2, const RealType &shift2) {
    RealType x, rho;
    Uniform unif_ab(lower, upper);
    do {
        x = unif_ab(eng);
        rho = std::exp(inv2s2 * (shift2 - (x - mu)*(x - mu)));
    } while (Unif01()(eng) > rho);
    return x;
}

/** Perform rejection sampling using an exponential distribution.  This can only be used for
 * single-tail truncation regions, and performs increasingly well the further out the truncation
 * range lies in the tail of the normal distribution.
 *
 * For a two-sided truncation (that is, with a finite value of the further-from-`mu` limit), the
 * algorithm first uses simple rejection sampling to draw a proposal value inside `[lower,upper]`,
 * then uses acceptance sampling calculation to decide whether to use that value.
 *
 * \param eng the random engine to use to obtain random values
 * \param sigma the standard deviation parameter of the pre-truncation normal distribution.
 * \param lower the lower bound of the truncated range (should be finite)
 * \param upper the upper bound of the truncated range (should be finite)
 * \param right_tail if true, draws are from the right tail (and so `mu <= lower <= upper`);
 * otherwise, draws are from the left tail (and so `lower <= upper <= mu`).
 * \param bound_dist the distance from `mu` to the closer boundary.  This is an unsigned (i.e.
 * always positive) value.
 * \param proposal_param a pre-computed value that determines the parameter of the proposal
 * exponential distribution.  We draw proposal values from an Exponential(proposal/sigma^2).  The
 * optimal value, in terms of requiring the fewest random draws, equals `(a + sqrt(a^2 + 4
 * sigma^2))/2`, where `a` is the value of `bound_dist`.  That value, however, is expensive to
 * compute, and so it is often more efficient to use an approximation such as just `a`, that is,
 * `bound_dist`.  An inexact approximation here has an efficiency effect (the better the
 * approximation, the more likely a drawn value is admissable), but not a distributional effect:
 * drawn values will still follow the desired truncated normal distribution.
 */
template <class Engine, class RealType, class Exponential = eris::random::exponential_distribution<RealType>>
RealType truncnorm_rejection_exponential(Engine &eng, const RealType &sigma, const RealType &lower, const RealType &upper,
        bool right_tail, const RealType &bound_dist, const RealType &proposal_param) {

    Exponential exponential;
    const RealType exp_max_times_sigma = upper - lower;
    const RealType twice_sigma_squared = RealType(2) * sigma * sigma;
    const RealType x_scale = sigma * sigma / proposal_param;
    const RealType x_delta = bound_dist - proposal_param;
    RealType x;
    do {
        // For 2-sided truncation, redraw z until we get one that won't exceed the
        // outer limit
        do { x = exponential(eng) * x_scale; } while (x > exp_max_times_sigma);

        // If we accept, we're accepting lower+x.  Our acceptance probability is
        //
        // rho(x) = exp(-(x - proposal_param)^2 / (2 sigma^2))
        //
        // Or, logically: accept if U01 < exp(-(x - proposal_param)^2 / (2 sigma^2))
        // Some more algebra yields:
        // log(U01) < -(x - proposal_param)^2 / (2 sigma^2)
        // (2 sigma^2) (-log(U01)) > (x - proposal_param)^2
        //
        // Now a neat trick: note that -log(U01) is actually just inverse CDF sampling of a Exp(1),
        // and since inverse CDF sampling is just one way to draw from a distribution, we can
        // replace it with a draw from Exp(1).  And so, in the end, we get the condition below (but
        // note that the inequality is reversed, because this is don't-accept condition).
    } while (twice_sigma_squared * exponential(eng) <= (x+x_delta)*(x+x_delta));

    return right_tail ? lower + x : upper - x;
}

} // namespace detail

/**
 * Instantiations of truncated_normal model a normal distribution, truncated to a given region
 * `[a,b]` (with open intervals as appropriate if a or b is infinite).  Such a distribution produces
 * random numbers `x` distributed with probability density function
 * \f[
 *   f(x) = \begin{array}{ll}
 *   \frac{1}{\Phi(\frac{b-\mu}{\sigma}) - \Phi(\frac{a-\mu}{\sigma})}
 *      \frac{1}{\sqrt{2\pi}\sigma} e^{-\frac{(x-\mu)^2}{2\sigma^2}} & \mbox{if } a \leq x \leq b \\
 *   0 & \mbox{otherwise}
 *   \end{array}
 * \f]
 * where \f$\mu\f$, \f$\sigma\f$, `a`, and `b` are the parameters of the distribution, and
 * \f$\Phi(\cdot)\f$ is the cumulative density function of the standard normal distribution.
 */
template<class RealType = double>
class truncated_normal_distribution {
// Some comments about the implementation: the code is made a little more complicated by not
// dividing by sigma (for various conditions) except when absolutely necessary, which is often isn't
// (we can multiply the other side of the condition by sigma instead).  Having tried it both ways,
// not doing the extra division improves performance in all the cases that don't actually need it.
// Hence we have constants like `_er_lambda_times_sigma` instead of just `_er_lambda`.
private:
public:
    typedef RealType input_type;
    typedef RealType result_type;

    class param_type {
    public:
        typedef truncated_normal_distribution distribution_type;

        /**
         * Constructs a @c param_type with a given mean, standard deviation, and truncation points.
         *
         * Requires: sigma >= 0 and a <= b.
         */
        explicit param_type(RealType mean_arg = RealType(0.0),
                            RealType sigma_arg = RealType(1.0),
                            RealType lower_limit_arg = -std::numeric_limits<RealType>::infinity(),
                            RealType upper_limit_arg = +std::numeric_limits<RealType>::infinity())
          : _mean(mean_arg),
            _sigma(sigma_arg),
            _lower_limit(lower_limit_arg),
            _upper_limit(upper_limit_arg)
        {}

        /** Returns the mean of the distribution. */
        RealType mean() const { return _mean; }

        /** Returns the standand deviation of the distribution. */
        RealType sigma() const { return _sigma; }

        /** Returns the lower truncation point of the distribution (or negative infinity if there is
         * no lower limit).
         */
        RealType lower_limit() const { return _lower_limit; }

        /** Returns the upper truncation point of the distribution (or positive infinity if there is
         * no upper limit).
         */
        RealType upper_limit() const { return _upper_limit; }

        /** Writes a @c param_type to a @c std::ostream. */
        BOOST_RANDOM_DETAIL_OSTREAM_OPERATOR(os, param_type, parm)
        { os << parm._mean << " " << parm._sigma << " " << parm._lower_limit << " " << parm._upper_limit; return os; }

        /** Reads a @c param_type from a @c std::istream. */
        BOOST_RANDOM_DETAIL_ISTREAM_OPERATOR(is, param_type, parm)
        { is >> parm._mean >> std::ws >> parm._sigma >> std::ws >> parm._lower_limit >> std::ws >> parm._upper_limit; return is; }

        /** Returns true if the two sets of parameters are the same. */
        BOOST_RANDOM_DETAIL_EQUALITY_OPERATOR(param_type, lhs, rhs) {
            return lhs._mean == rhs._mean and lhs._sigma == rhs._sigma
                and lhs._lower_limit == rhs._lower_limit and lhs._upper_limit == rhs._upper_limit;
        }

        /** Returns true if the two sets of parameters are the different. */
        BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(param_type)

    private:
        RealType _mean, _sigma, _lower_limit, _upper_limit;
    };

    /**
     * Constructs a @c normal_distribution object. @c mean and @c sigma are the parameters of the
     * untruncated normal distribution, @c lower_limit and @c upper_limit are the truncation
     * boundaries for the truncated distribution.
     *
     * Requires: sigma >= 0
     */
    explicit truncated_normal_distribution(
            const RealType& mean = RealType(0.0),
            const RealType& sigma = RealType(1.0),
            const RealType& lower_limit = -std::numeric_limits<RealType>::infinity(),
            const RealType& upper_limit = +std::numeric_limits<RealType>::infinity())
      : _mean(mean), _sigma(sigma), _lower_limit(lower_limit), _upper_limit(upper_limit)
    {
        BOOST_ASSERT(_sigma >= RealType(0) and _lower_limit <= _upper_limit);
    }

    /**
     * Constructs a @c normal_distribution object from its parameters.
     */
    explicit truncated_normal_distribution(const param_type& parm)
      : _mean(parm.mean()), _sigma(parm.sigma()), _lower_limit(parm.lower_limit()), _upper_limit(parm.upper_limit())
    {}

    /**  Returns the mean of the distribution. */
    RealType mean() const { return _mean; }
    /** Returns the standard deviation of the distribution. */
    RealType sigma() const { return _sigma; }

    /** Returns the smallest value that the distribution can produce. */
    RealType min BOOST_PREVENT_MACRO_SUBSTITUTION () const { return _lower_limit; }
    /** Returns the largest value that the distribution can produce. */
    RealType max BOOST_PREVENT_MACRO_SUBSTITUTION () const { return _upper_limit; }

    /** Returns the parameters of the distribution. */
    param_type param() const { return param_type(_mean, _sigma, _lower_limit, _upper_limit); }
    /** Sets the parameters of the distribution. */
    void param(const param_type& parm)
    {
        // If we have already calculated _method, make sure that the given parameters are actually
        // different before we clear it (and thus require a recalculation)
        if (_method == Method::UNKNOWN or parm != param()) {
            _mean = parm.mean();
            _sigma = parm.sigma();
            _lower_limit = parm.lower_limit();
            _upper_limit = parm.upper_limit();
            _method = Method::UNKNOWN;
        }
    }

    /**
     * Effects: Subsequent uses of the distribution do not depend on values produced by any engine
     * prior to invoking reset.
     */
    void reset() { }

    /**  Returns a truncated normal variate. */
    template<class Engine>
    result_type operator()(Engine& eng)
    {
        if (_method == Method::UNKNOWN) _determine_method();
        switch (_method) {
            case Method::TRIVIAL:     return _lower_limit; // == __upper_limit
            case Method::NORMAL:      return detail::truncnorm_rejection_normal(eng, _mean, _sigma, _lower_limit, _upper_limit);
            case Method::HALFNORMAL:  return detail::truncnorm_rejection_halfnormal(eng, _mean, _sigma, _lower_limit, _upper_limit, _right_tail);
            case Method::UNIFORM:     return detail::truncnorm_rejection_uniform(eng, _mean, _lower_limit, _upper_limit, _ur_inv_2_sigma_squared, _ur_shift2);
            case Method::EXPONENTIAL: return detail::truncnorm_rejection_exponential(eng, _sigma, _lower_limit, _upper_limit, _right_tail, _er_a, _er_lambda_times_sigma);
            // Should be impossible (but silences the unhandled-switch-case warning):
            case Method::UNKNOWN: ;
        }

        // Shouldn't reach here
        BOOST_ASSERT(false);
    }

    /** Returns a normal variate with parameters specified by @c param. */
    template<class URNG>
    result_type operator()(URNG& urng, const param_type& parm)
    {
        return truncated_normal_distribution(parm)(urng);
    }

    /** Writes a @c truncated_normal_distribution to a @c std::ostream. */
    BOOST_RANDOM_DETAIL_OSTREAM_OPERATOR(os, truncated_normal_distribution, d)
    {
        os << d._mean << " " << d._sigma << " " << d._lower_limit << " " << d._upper_limit;
        return os;
    }

    /** Reads a @c truncated_normal_distribution from a @c std::istream. */
    BOOST_RANDOM_DETAIL_ISTREAM_OPERATOR(is, truncated_normal_distribution, d)
    {
        // Do this through param_type so that it can avoid updating _method if the new values don't
        // actually change anything.
        param_type new_param;
        is >> new_param;
        d.param(new_param);
        return is;
    }

    /**
     * Returns true if the two instances of @c truncated_normal_distribution will
     * return identical sequences of values given equal generators.
     */
    BOOST_RANDOM_DETAIL_EQUALITY_OPERATOR(truncated_normal_distribution, lhs, rhs)
    {
        return lhs._mean == rhs._mean and lhs._sigma == rhs._sigma
            and lhs._lower_limit == rhs._lower_limit
            and lhs._upper_limit == rhs._upper_limit;
    }

    /**
     * Returns true if the two instances of @c truncated_normal_distribution will
     * return different sequences of values given equal generators.
     */
    BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(truncated_normal_distribution)

private:
    RealType _mean, _sigma, _lower_limit, _upper_limit;

    // The methods that we use to draw the truncated normal.
    //
    // UNKNOWN - method hasn't been determined yet (calling _determine_method() will update it)
    // TRIVIAL - lower == upper, so truncated distribution is the constant `lower` (`== upper`).
    // NORMAL - normal rejection sampling
    // HALFNORMAL - like NORMAL, but confined to one tail, and so draws from the unwanted tail can
    // be translated to the wanted tail.
    // UNIFORM - uniform rejection sampling -- generally used when the range is quite small
    // EXPONENTIAL - exponential rejection sampling -- used when the truncation range is in the
    // non-central part of one tail.
    // (NB: this is just an enum class in c++11, and an emulation of it in pre-c++11)
    BOOST_SCOPED_ENUM_DECLARE_BEGIN(Method) {
        UNKNOWN, TRIVIAL, NORMAL, HALFNORMAL, UNIFORM, EXPONENTIAL
    }
    BOOST_SCOPED_ENUM_DECLARE_END(Method)

    Method _method = Method::UNKNOWN;

    // Called at the beginning of a draw to check whether _method is UNKNOWN; if so, it (and
    // possibly the cache variables below) are updated as needed for generating a value.
    void _determine_method() {
        // We have two main cases (plus a mirror of one of them):
        // 1 - truncation bounds are on either side of the mean
        // 2&3 - truncation range is entirely on one side of the mean
        //
        // Case 0 (not really one of the cases): a == b, TRIVIAL (the "distribution" is a constant)
        // Case 1: a < mu < b
        //   (a) if b-a < sigma*ur_below_nr_above then UNIFORM
        //   (b) else NORMAL
        // Case 2: mean <= a < b
        //   (a) if a <= mean + sigma*hr_below_er_above:
        //     (i) if (b-mean)/sigma < f_1((a-mean)/sigma) then UNIFORM
        //     (ii) else HALFNORMAL
        //   (b) else (i.e. a > sigma*hr_below_er_above):
        //     (i) if (b-mean)/sigma < f_2((a-mean)/sigma) then UNIFORM
        //     (ii) else EXPONENTIAL
        // Case 3: a < b <= mean: this is exactly case 2, but reflected across the mean.
        //
        // We have 7+4 cases/subcases to handle (7 proper cases, plus 4 more that are just symmetric
        // versions of proper cases).
        //
        // (0) is trivial
        // (1b) uses normal rejection sampling
        // (2aii,3aii) use half normal rejection sampling
        // (2bii,3bii) use exponential rejection sampling
        // (1a,2ai,2bi,3ai,3bi) use uniform rejection sampling

        // Case 0:
        if (_lower_limit == _upper_limit) {
            _method = Method::TRIVIAL;
        }
        // Case 1:
        else if (_lower_limit < _mean and _mean < _upper_limit) {
            if (_upper_limit - _lower_limit < _sigma * detail::truncnorm_threshold<RealType>::ur_below_nr_above) {
                // The truncation region is small enough that uniform rejection sampling beats
                // normal rejection sampling
                _method = Method::UNIFORM;
                _ur_shift2 = RealType(0);
                _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
            }
            else {
                // The truncation region is big enough to just do simple normal rejection sampling
                _method = Method::NORMAL;
            }
        }
        // Case 2 or 3:
        else {
            // a and b are demeaned limits, and may also be mean-flipped (to ensure 0 <= a < b)
            RealType a, b;
            if (_lower_limit >= _mean) {
                // Truncation range is already in the right tail, so don't need to mean flip; just demean:
                _right_tail = true;
                a = _lower_limit - _mean;
                b = _upper_limit - _mean;
            }
            else {
                // Truncation range is in the left tail, so do a mean flip (to cut the number of cases in half)
                _right_tail = false;
                // Mean flipping a demeaned value is just negating it (but negating/mean flipping
                // also reverses the roles of the upper/lower limits):
                a = _mean - _upper_limit;
                b = _mean - _lower_limit;
            }

            // Now 0 <= a < b (we'll swap back at the end if we need a left-tail draw)

            if (a <= _sigma * detail::truncnorm_threshold<RealType>::hr_below_er_above) {
                // a is not too large: we resort to either halfnormal rejection sampling or, if its
                // acceptance rate would be too low (because b is too low), uniform rejection
                // sampling
                if (std::isinf(b)) {
                    _method = Method::HALFNORMAL;
                }
                else {
                    _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
                    _ur_shift2 = a*a;
                    if (b - a >= (
                                _sigma *
                                detail::truncnorm_threshold<RealType>::cost_hr_rel_ur *
                                boost::math::constants::root_half_pi<RealType>() *
                                detail::exp_maclaurin(_ur_shift2 * _ur_inv_2_sigma_squared)
                                )
                       ) {
                        // b is big (so [a,b] is likely enough to occur that halfnormal rej is
                        // worthwhile)
                        _method = Method::HALFNORMAL;
                    }
                    else {
                        // Otherwise b is small; using uniform rejection sampling (which requires an exp
                        // call) is more efficient than the low acceptance rate of half-normal rejection
                        _method = Method::UNIFORM;
                    }
                }
            }
            else {
                // Otherwise we're sufficiently far out in the tail that ER is preferred to HR; the
                // only thing we still have to decide is whether UR is a better choice than ER.
                if (std::isinf(b) or a * (b-a) >= (_sigma * _sigma) * detail::truncnorm_threshold<RealType>::prefer_ur_multiplier) {
                    _method = Method::EXPONENTIAL;
                    _er_a = a;
                    _er_lambda_times_sigma = a;
                }
                else {
                    _method = Method::UNIFORM;
                    _ur_shift2 = a*a;
                    _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
                }
            }
        }
    }

    // True if the truncation range is in the right tail; if not, we handle this by pretending it's
    // in the right tail, then flipping the final result.  Used by HALFNORMAL and EXPONENTIAL.
    bool _right_tail;

    // Use a union because we won't need both of these at once, but being able to use descriptive
    // variable names is nice.
    union {
        // For _method == UNIFORM, this is the constant term in the exp (before division by 2 sigma^2).
        // For a truncation range including the mean, this is 0; otherwise this is (a - mu)^2, where a
        // is the truncation point closer to the mean.
        RealType _ur_shift2;

        // For EXPONENTIAL, this stores the non-divided-by-sigma lambda value of the exponential
        // (i.e. we want to draw exponential(thisvalue/sigma).  The best acceptance rate is when
        // this value divided by sigma equals (alpha+sqrt(alpha^2+4))/2, but for large alpha, that
        // is approximately equal to alpha, and so in that case we just use a (=alpha*sigma) to
        // avoid the sqrt.
        RealType _er_lambda_times_sigma;
    };

    union {
        // The cached value of 1/(2*sigma^2), so that we can avoid repeating the division (for uniform
        // rejection).
        RealType _ur_inv_2_sigma_squared;

        // The cached value of a, the demeaned (but not normalized) truncation point nearer the mean
        RealType _er_a;
    };

};

}} // namespace eris::random
