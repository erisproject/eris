#pragma once
#include <limits>
#include <boost/random/detail/operators.hpp>
#include <eris/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/uniform_01.hpp>
#include <eris/random/exponential_distribution.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/core/scoped_enum.hpp>
#include <eris/random/detail/truncated_normal_thresholds.hpp>

namespace eris { namespace random {

namespace detail {

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
 * \param signed_sigma the distribution parameter of the halfnormal distribution (for an
 * untruncated, full normal distribution, this is the standard deviation), with positive sign for a
 * right-tail draw, negative sign for a left-tail draw.
 * \param lower the lower truncation point
 * \param upper the upper truncation point
 */
template <class Engine, class RealType, class Normal = eris::random::normal_distribution<RealType>>
RealType truncnorm_rejection_halfnormal(Engine &eng, const RealType &mu, const RealType &signed_sigma, const RealType &lower, const RealType &upper) {
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
        using std::exp;
        x = unif_ab(eng);
        rho = exp(inv2s2 * (shift2 - (x - mu)*(x - mu)));
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
 * \param mu the mean parameter of the pre-truncation normal distribution.
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
RealType truncnorm_rejection_exponential(Engine &eng, const RealType &mu, const RealType &sigma,
        const RealType &lower, const RealType &upper, const RealType &bound_dist, const RealType &proposal_param) {

    Exponential exponential;
    const RealType exp_max_times_sigma = upper - lower;
    const RealType twice_sigma_squared = RealType(2) * (sigma * sigma);
    const RealType x_scale = (sigma * sigma) / proposal_param;
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

    return lower >= mu ? lower + x : upper - x;
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
// dividing by sigma (for various conditions) except when absolutely necessary, which it often isn't
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
         * Constructs a @c param_type with a given (pre-truncation) mean, standard deviation, and truncation points.
         *
         * Requires: sigma >= 0 and a <= b.
         */
        explicit param_type(RealType mu_arg = RealType(0.0),
                            RealType sigma_arg = RealType(1.0),
                            RealType lower_limit_arg = -std::numeric_limits<RealType>::infinity(),
                            RealType upper_limit_arg = +std::numeric_limits<RealType>::infinity())
          : _mu(mu_arg),
            _sigma(sigma_arg),
            _lower_limit(lower_limit_arg),
            _upper_limit(upper_limit_arg)
        {}

        /** Returns the mean parameter of the distribution. */
        RealType mu() const { return _mu; }

        /** Returns the standand deviation parameter of the distribution. */
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
        { os << parm._mu << " " << parm._sigma << " " << parm._lower_limit << " " << parm._upper_limit; return os; }

        /** Reads a @c param_type from a @c std::istream. */
        BOOST_RANDOM_DETAIL_ISTREAM_OPERATOR(is, param_type, parm)
        { is >> parm._mu >> std::ws >> parm._sigma >> std::ws >> parm._lower_limit >> std::ws >> parm._upper_limit; return is; }

        /** Returns true if the two sets of parameters are the same. */
        BOOST_RANDOM_DETAIL_EQUALITY_OPERATOR(param_type, lhs, rhs) {
            return lhs._mu == rhs._mu and lhs._sigma == rhs._sigma
                and lhs._lower_limit == rhs._lower_limit and lhs._upper_limit == rhs._upper_limit;
        }

        /** Returns true if the two sets of parameters are the different. */
        BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(param_type)

    private:
        RealType _mu, _sigma, _lower_limit, _upper_limit;
    };

    /**
     * Constructs a @c normal_distribution object. @c mu and @c sigma are the parameters of the
     * untruncated normal distribution, @c lower_limit and @c upper_limit are the truncation
     * boundaries for the truncated distribution.
     *
     * Requires: sigma >= 0
     */
    explicit truncated_normal_distribution(
            const RealType& mu = RealType(0.0),
            const RealType& sigma = RealType(1.0),
            const RealType& lower_limit = -std::numeric_limits<RealType>::infinity(),
            const RealType& upper_limit = +std::numeric_limits<RealType>::infinity())
      : _mu(mu), _sigma(sigma), _lower_limit(lower_limit), _upper_limit(upper_limit)
    {
        BOOST_ASSERT(_sigma >= RealType(0) and _lower_limit <= _upper_limit);
    }

    /**
     * Constructs a @c normal_distribution object from its parameters.
     */
    explicit truncated_normal_distribution(const param_type& parm)
      : _mu(parm.mu()), _sigma(parm.sigma()), _lower_limit(parm.lower_limit()), _upper_limit(parm.upper_limit())
    {}

    /**  Returns the mean parameter of the distribution. Note that this is not necessarily the
     * distribution mean, as the truncation boundaries can alter the mean.
     */
    RealType mu() const { return _mu; }
    /** Returns the standard deviation parameter of the distribution. Note that this is not
     * necessarily the standard deviation of the distribution itself: the truncation boundaries
     * also affect that.
     */
    RealType sigma() const { return _sigma; }

    /** Returns the smallest value that the distribution can produce. */
    RealType min BOOST_PREVENT_MACRO_SUBSTITUTION () const { return _lower_limit; }
    /** Returns the largest value that the distribution can produce. */
    RealType max BOOST_PREVENT_MACRO_SUBSTITUTION () const { return _upper_limit; }

    /** Returns the parameters of the distribution. */
    param_type param() const { return param_type(_mu, _sigma, _lower_limit, _upper_limit); }
    /** Sets the parameters of the distribution. */
    void param(const param_type& parm)
    {
        // If we have already calculated _method, make sure that the given parameters are actually
        // different before we clear it (and thus require a recalculation)
        if (_method == Method::UNKNOWN or parm != param()) {
            _mu = parm.mu();
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
            case Method::NORMAL:      return detail::truncnorm_rejection_normal(eng, _mu, _sigma, _lower_limit, _upper_limit);
            case Method::HALFNORMAL:  return detail::truncnorm_rejection_halfnormal(eng, _mu, _hr_signed_sigma, _lower_limit, _upper_limit);
            case Method::UNIFORM:     return detail::truncnorm_rejection_uniform(eng, _mu, _lower_limit, _upper_limit, _ur_inv_2_sigma_squared, _ur_shift2);
            case Method::EXPONENTIAL: return detail::truncnorm_rejection_exponential(eng, _mu, _sigma, _lower_limit, _upper_limit, _er_a, _er_lambda_times_sigma);
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
        os << d._mu << " " << d._sigma << " " << d._lower_limit << " " << d._upper_limit;
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
        return lhs._mu == rhs._mu and lhs._sigma == rhs._sigma
            and lhs._lower_limit == rhs._lower_limit
            and lhs._upper_limit == rhs._upper_limit;
    }

    /**
     * Returns true if the two instances of @c truncated_normal_distribution will
     * return different sequences of values given equal generators.
     */
    BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(truncated_normal_distribution)

private:
    RealType _mu, _sigma, _lower_limit, _upper_limit;

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
    //
    // TODO: it might be worthwhile exposing this as a public method, and adapting it to accept a
    // `quick` argument: if true, it would do as it does now, using approximations; if false, it
    // could compute a more exact threshold with better long-run performance (at the expense of
    // up-front cost).
    void _determine_method() {
        // We have two main cases (plus a mirror of one of them):
        // 1 - truncation bounds are on either side of mu
        // 2&3 - truncation range is entirely on one side of mu
        //
        // Case 0 (not really one of the cases): a == b, TRIVIAL (the "distribution" is a constant)
        // Case 1: a < mu < b
        //   (a) if b-a < sigma*ur_below_nr_above then UNIFORM
        //   (b) else NORMAL
        // Case 2: mu <= a < b
        //   (a) if a <= mu + sigma*hr_below_er_above:
        //     (i) if (b-mu)/sigma < f_1((a-mu)/sigma) then UNIFORM
        //     (ii) else HALFNORMAL
        //   (b) else (i.e. a > sigma*hr_below_er_above):
        //     (i) if (b-mu)/sigma < f_2((a-mu)/sigma) then UNIFORM
        //     (ii) else EXPONENTIAL
        // Case 3: a < b <= mu: this is exactly case 2, but reflected across mu.
        //
        // We have 7+4 cases/subcases to handle (7 proper cases, plus 4 more that are just symmetric
        // versions of proper cases).
        //
        // (0) is trivial
        // (1b) uses normal rejection sampling
        // (2aii,3aii) use half normal rejection sampling
        // (2bii,3bii) use exponential rejection sampling
        // (1a,2ai,2bi,3ai,3bi) use uniform rejection sampling

        double a, b; // Magnitudes into the tail for case 2/3

        // Case 0:
        if (_lower_limit == _upper_limit) {
            _method = Method::TRIVIAL;
            return;
        }
        else if (_lower_limit < _mu) {
            // Case 1: lower < mu < upper:
            if (_upper_limit > _mu) {
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
                return;
            }

            // Case 3: lower < upper <= mu
            else {
                // Mean flipping a demeaned value is just negating it (but negating/mean flipping
                // also reverses the roles of the upper/lower limits).  This guarantees we get:
                // 0 <= a < b, i.e. we can use a and b as if they are in the right tail.
                a = _mu - _upper_limit;
                b = _mu - _lower_limit;
            }
        }
        // Case 2: mu <= lower < upper
        else {
            // Truncation range is already in the right tail, so don't need to mean flip as
            // above, but just demean:
            a = _lower_limit - _mu;
            b = _upper_limit - _mu;
        }

        using std::isinf;

        // Now we're either in case 2 or three, and a and b are the (absolute value) distances from
        // the mean of the near and far limits in the relevant positive tail.
        if (a <= _sigma * detail::truncnorm_threshold<RealType>::hr_below_er_above) {
            // a is not too large: we resort to either halfnormal rejection sampling or, if its
            // acceptance rate would be too low (because b is too low), uniform rejection
            // sampling
            if (isinf(b) or b - a >= detail::truncnorm_threshold<RealType>::ur_hr_threshold(a, _sigma)) {
                _method = Method::HALFNORMAL;
                _hr_signed_sigma = _lower_limit >= _mu ? _sigma : -_sigma;
            }
            else {
                // Otherwise b is small; using uniform rejection sampling (which requires an exp
                // call) is more efficient than the low acceptance rate of half-normal rejection
                _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
                _ur_shift2 = a*a;
                _method = Method::UNIFORM;
            }
        }
        else {
            // Otherwise we're sufficiently far out in the tail that ER is preferred to HR; we
            // now have to decide whether UR is preferred to ER, and if not, whether the
            // simplified-parameter ER is preferred to the acceptance-optimal parameter.
            if (isinf(b) or a * (b-a) >= (_sigma * _sigma) * detail::truncnorm_threshold<RealType>::prefer_ur_multiplier) {
                _method = Method::EXPONENTIAL;
                using std::sqrt;
                _er_a = a;
                _er_lambda_times_sigma = (a < _sigma * detail::truncnorm_threshold<RealType>::er_approximate_above)
                    // Relatively small a: calculating this thing is worthwhile:
                    ? 0.5 * (a + sqrt(a*a + 4*(_sigma * _sigma)))
                    : a; // a is large, so better to use a, avoid the calculation, and incur the potential extra discards
            }
            else {
                _method = Method::UNIFORM;
                _ur_shift2 = a*a;
                _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
            }
        }
    }

    // Use a union because we won't need both of these at once, but being able to use descriptive
    // variable names is nice.
    union {
        // For _method == UNIFORM, this is the constant term in the exp (before division by 2 sigma^2).
        // For a truncation range including the mean, this is 0; otherwise this is (a - mu)^2, where a
        // is the truncation point closer to the mean.
        RealType _ur_shift2;

        // For EXPONENTIAL, this stores the non-divided-by-sigma lambda value of the exponential
        // (i.e. we want to draw exponential(thisvalue/sigma).  The best acceptance rate is when
        // this value divided by sigma equals (alpha+sqrt(alpha^2+4))/2, but calculating that is
        // expensive: currently, we just set this to a (=alpha*sigma), which is a decent
        // approximation as long as we aren't too far into the central region, and saves the
        // calculations above.
        RealType _er_lambda_times_sigma;

        // For Half-normal, this is _sigma (for right-tail half-normal draws) or -_sigma (for
        // left-tail draws).
        RealType _hr_signed_sigma;
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
