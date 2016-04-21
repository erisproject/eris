#pragma once
#include <limits>
#include <boost/random/detail/operators.hpp>
#include <eris/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/uniform_01.hpp>
#include <eris/random/exponential_distribution.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/core/scoped_enum.hpp>
#include <iostream>

namespace eris { namespace random {

namespace detail {

// Constants for the truncated normal algorithm.  All of these values depend on the specific CPU,
// architecture, compiler, etc., so we only try to provide ballpark figures here that should work
// reasonably well.
template<class RealType>
struct truncnorm_threshold {
    /* The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use exponential
     * rejection sampling instead of half normal rejection sampling.  This value applies when the
     * truncation range is contained entirely within one side of the distribution.  (Note, however,
     * that on either side of this condition, we may still resort to uniform rejection sampling if b
     * is too close to a).
     *
     * The optimal value depends on the speed of:
     *     - drawing from a normal distribution
     *     - drawing from an exponential distribution
     *     - drawing from a uniform distribution
     *     - calculating sqrt()
     *     - calculating exp()
     */
    static constexpr RealType hr_below_er_above = RealType(0.7565);

    /* The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use a numerical
     * approximation of a complex term in the ER/UR decision threshold.
     *
     * In particular, this uses the simplification:
     *
     *     1/a =~ 2/(a+sqrt(a^2+4))*exp((a^2-a*sqrt(a^2+4))/4 + 1/2)
     *
     * which holds increasingly well as a increases.  The value of this constant is determined by the
     * value of a at which the increased expected cost of the less efficient UR calculation just equals
     * the fixed cost of calculating the above expression.
     *
     * NB: if this value is below `hr_below_er_above`, as is often is for a library with fast normal
     * generation, the simplification is always used (because the ER/UR decision only matters for
     * values above that threshold).
     *
     * This value depends on the speed of:
     *     - drawing from an exponential distribution
     *     - drawing from a uniform distribution
     *     - calculating sqrt()
     *     - calculating exp()
     */
    static constexpr RealType simplify_er_ur_above = RealType(0.6234);

    /* The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use a numerical
     * approximation of a complex term in the ER calculation.
     *
     * In particular, above this value of \f$\alpha\f$, \f$\lambda = \alpha\f$ is used instead of
     * $\lambda = \frac{1}{2} (\alpha + std::sqrt(\alpha^2 + 4))\f$.  Although this results in a slightly
     * less optimal exponential distribution, for large values of \f$\alpha\f$ (i.e. above this
     * constant), the average efficiency loss is less than the cost of calculating the square root.
     *
     * This value depends on the speed of:
     *     - drawing from an exponential distribution
     *     - drawing from a uniform distribution
     *     - calculating sqrt()
     *     - calculating exp()
     */
    static constexpr RealType simplify_er_lambda_above = RealType(1.6193);

    /* The Taylor expansion order to use when approximating e^x when deciding between UR and HR.  The
     * minimum required approximation order depends on the specific value of `hr_below_er_above`
     * and the speed of performing the approximation calculations; it is typically 1 or 2, but 2 or 3
     * are often a better choice.
     *
     * draw-perf reports a few things in this regards: first, it reports whether a T1, T2, or higher
     * order approximation is required for using the approximation to be at least as good as calculating
     * the precise optimal value; this value should be no smaller than that.  For example, in my testing,
     * for boost and gsl-zigg I get a value of 2, for the others a value of 1.
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
     * some accurancy near 0 for some accuracy near a0, but moreover is only slightly more accurate than
     * T3 around a0, and much worse around 0; better, then, to use use T3.  (The story between T3@a and
     * T4@0 is similar).  So again, not worth the effort.
     *
     * The current implementation supports values from 1 to 8 (and compiles away the 7 unused cases);
     * the default is 3.
     */
    static constexpr int ur_hr_approximation_order = 3;

    /* This is the threshold value for deciding between NR and UR b-a: when b-a is above this,
     * rejection sampling from a normal distribution is preferred; below this, uniform rejection
     * sampling is preferred.
     *
     * This value equals sqrt(2*pi) times the ratio of normal draw cost to uniform sampling iteration
     * cost (remember that a uniform sample iteration typically requires drawing 2 uniforms and
     * evaluating one exponential, while normal rejection simply involves drawing a normal).
     */
    static constexpr RealType ur_below_nr_above = RealType(1.4621);

    /* This is the average cost of a *single iteration* of the drawing and accept/reject calculation
     * for exponential rejection sampling, relative to the equivalent cost of uniform rejection
     * sampling.  (Obtaining an accepted draw may require more than one iteration, but that
     * probability is handled externally to this value.)
     */
    static constexpr RealType cost_er_rel_ur = RealType(0.9963);

    /** This is the average cost of a single normal or halfnormal draw, relative to the cost of a
     * single uniform accept/reject iteration.
     */
    static constexpr RealType cost_nr_rel_ur = RealType(0.5833);
};

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
        typedef boost::random::uniform_01<RealType> Unif01;

        if (_method == Method::UNKNOWN) _determine_method();
        switch (_method) {
            // lower == upper, so distribution is a trivial constant
            case Method::TRIVIAL:
                return _lower_limit;

            // Normal rejection sampling: just sample from normal until we get an admissable value
            case Method::NORMAL:
                {
                    RealType x;
                    normal_distribution<RealType> normal(_mean, _sigma);
                    do { x = normal(eng); } while (x < _lower_limit or x > _upper_limit);
                    return x;
                }

            // Half-normal rejection sampling: sample normal values, but convert any wrong-tail
            // draws to the opposite tail; repeat until we get an admissable value.
            case Method::HALFNORMAL:
                {
                    const RealType signed_sigma(_left_tail ? -_sigma : _sigma);
                    RealType x;
                    do {
                        x = _mean + signed_sigma*fabs(normal_distribution<RealType>()(eng));
                    } while (x < _lower_limit or x > _upper_limit);
                    return x;
                }

            // Uniform: rejection sampling using a uniform and proportional acceptance rate
            case Method::UNIFORM:
                {
                    RealType x, rho;
                    boost::random::uniform_real_distribution<RealType> unif_ab(_lower_limit, _upper_limit);
                    do {
                        x = unif_ab(eng);
                        rho = std::exp(_ur_inv_2_sigma_squared * (_ur_shift - (x - _mean)*(x - _mean)));
                    } while (Unif01()(eng) > rho);
                    return x;
                }

            // Exponential: used out in the tails; accept with appropriate acceptance rate
            case Method::EXPONENTIAL:
                {
                    exponential_distribution<RealType> exponential;
                    const RealType exp_max_times_sigma = _upper_limit - _lower_limit;
                    const RealType y_scale = 2 * _sigma;
                    const RealType x_scale = _sigma / _er_lambda_times_sigma;
                    const RealType x_delta = _er_a - _er_lambda_times_sigma;
                    RealType x;
                    do {
                        // For 2-sided truncation, redraw z until we get one that won't exceed the
                        // outer limit; note that, if x is accepted, our final value will be
                        // (alpha + x/alpha)*sigma + mean, where alpha == a/sigma, the
                        // closer-to-mean limit (and positive; we handle the left tail at the end).
                        // Requiring that this be less than the outer limit is equivalent to
                        // requiring x < alpha*((up-low) / sigma).  The left tail yields exactly the
                        // same condition.
                        do { x = exponential(eng) * x_scale; } while (_sigma * x > exp_max_times_sigma);
                        // x -> x + a - lambda
                    } while (exponential(eng) * y_scale <= (x+x_delta)*(x+x_delta));

                    return _left_tail ? _upper_limit - x*_sigma : _lower_limit + x*_sigma;
                }

            // Should be impossible (but silences the unhandled-switch-case warning):
            case Method::UNKNOWN:
                ;
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
        // Case 0 (not really one of the cases): a == b, TRIVIAL ("distribution" is a constant)
        // Case 1: a < mean < b
        //   (a) if b-a < sigma*ur_below_nr_above then UNIFORM
        //   (b) else NORMAL
        // Case 2: mean <= a < b
        //   (a) if a <= mean + sigma*hr_below_er_above:
        //     (i) if (b-mean)/sigma < f_1((a-mean)/sigma) then UNIFORM   <-- FIXME: can I easily avoid all of these divisions?
        //     (ii) else HALFNORMAL
        //   (b) else (i.e. a > sigma*hr_below_er_above):
        //     (i) if (b-mean)/sigma < f_2((a-mean)/sigma) then UNIFORM
        //     (ii) else EXPONENTIAL
        // Case 3: a < b <= mean: this is exactly case 2, but reflected across the mean.
        //
        // We have 7+4 cases/subcases to handle (7 proper cases, plus 4 more that are just symmetric
        // versions of the proper cases).
        //
        // 1 uses normal rejection sampling
        // 1+1 use half normal rejection sampling
        // 1+1 use exponential rejection sampling
        // 3+2 use uniform rejection sampling

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
                _ur_shift = RealType(0);
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
                _left_tail = false;
                a = _lower_limit - _mean;
                b = _upper_limit - _mean;
            }
            else {
                // Truncation range is in the left tail, so do a mean flip (to cut the number of cases in half)
                _left_tail = true;
                // Mean flipping a demeaned value is just negating it (but negating/mean flipping
                // also reverses the roles of the upper/lower limits):
                a = _mean - _upper_limit;
                b = _mean - _lower_limit;
            }

            // Now 0 <= a < b

            // 3rd-order Taylor approximation of e^x:
            auto T3 = [](const RealType &x) {
                return 1 + x*(1 + x*(1./2 + 1./6*x));
            };


            // FIXME: these are temporary
            auto f_1 = [this,&T3](const RealType &a) {
                // This division is unavoidable:
                const RealType alpha = a/_sigma;
                return a + boost::math::constants::root_half_pi<RealType>() *
                    T3(RealType(0.5)*alpha*alpha);
//                    std::exp(RealType(0.5)*alpha*alpha);
            };

            if (a <= _sigma * detail::truncnorm_threshold<RealType>::hr_below_er_above) {
                // a is not too large: we resort to either halfnormal rejection sampling or, if its
                // acceptance rate would be too low (because b is too low), uniform rejection
                // sampling
                if (std::isinf(b) or b >= _sigma * f_1(a)) {
                    // b is big (so [a,b] is likely enough to occur that halfnormal rej is
                    // worthwhile)
                    _method = Method::HALFNORMAL;
                }
                else {
                    // Otherwise b is small; using uniform rejection sampling (which requires an exp
                    // call) is more efficient than the low acceptance rate of half-normal rejection
                    _method = Method::UNIFORM;
                    _ur_shift = a*a;
                }
            }
            else {
                // Otherwise a is large enough that, even if b is infinite, halfnormal sampling
                // isn't worthwhile--i.e. we're too far out in the tail.  If the tail region is
                // large enough (where the "large enough" value of (b-a) decreases with a), exponential
                // rejection sampling is a good choice.  But if really small, it also has too low an
                // acceptance rate, and so we again resort to uniform rejection sampling.
                //
                // As for "large enough", the exactly condition depends on the expected costs
                // (taking into account the draw time required and acceptance rates) of ER vs UR, but it is
                // approximately equal to (b-a > 1/a), with this approximation getting better for
                // large a.
                //
                // The first thing to check is whether the 1/a approximation is good enough
                //
                // This first condition is comparing constexprs, so should be eliminated by any
                // decent compiler at compile time, either avoiding the 'a >=' check and eliminating
                // the else (when true), or just eliminated (when false).
                if (detail::truncnorm_threshold<double>::simplify_er_ur_above < detail::truncnorm_threshold<double>::hr_below_er_above
                        or a >= _sigma*detail::truncnorm_threshold<RealType>::simplify_er_ur_above) {
                    // beta - alpha < c * (1/alpha), with beta=b/sigma, alpha=a/sigma becomes:
                    if (b*a < a*a + _sigma*_sigma*detail::truncnorm_threshold<RealType>::cost_er_rel_ur) {
                        _method = Method::UNIFORM;
                        _ur_shift = a*a;
                    }
                    else {
                        _method = Method::EXPONENTIAL;
                        _er_a = a;
                        _er_lambda_times_sigma =
                            a >= _sigma * detail::truncnorm_threshold<RealType>::simplify_er_lambda_above
                            ? a
                            : RealType(0.5) * (a + sqrt(a*a + 4*_sigma*_sigma));
                    }
                }
                else {
                    // a is small enough that this expensive calculation is worthwhile (note: when
                    // the first condition in the above if is true, this entire branch is eliminated
                    // in favour of the above).

                    // This division is unavoidable: it's needed in several places in what follows
                    const RealType alpha = a/_sigma;
                    // Precalculate this, because it shows up twice in the messy calculation below:
                    const RealType sqrtalpha2p4 = sqrt(alpha*alpha + RealType(4));
                    // We can avoid one division, however; this is the denominator under the "2".
                    // It gets replaced with two multiplications.
                    const RealType rhs_denominator = alpha + sqrtalpha2p4;

                    // Our threshold condition is this thing:
                    //
                    // beta - alpha < c * 2/(alpha+sqrt(alpha^2+4)) * exp((alpha^2 - alpha*sqrt(alpha^2+4)) / 4 + 1/2)
                    //
                    // where alpha = a/sigma, beta = b/sigma.  Eliminating divisions leaves:
                    if ((b-a)*rhs_denominator < _sigma * detail::truncnorm_threshold<RealType>::cost_er_rel_ur *
                            RealType(2) * std::exp(RealType(0.5) + RealType(0.25) * (alpha*alpha - alpha*sqrtalpha2p4))
                       ) {
                        _method = Method::UNIFORM;
                        _ur_shift = a*a;
                    }
                    else {
                        _method = Method::EXPONENTIAL;
                        _er_a = a;
                        // We've already had to calculate the sqrt, so use it (i.e. replacing this
                        // with the approximation of a buys us nothing).
                        _er_lambda_times_sigma = 0.5 * (a + _sigma*sqrtalpha2p4);
                    }
                }
            }
        }

        if (_method == Method::UNIFORM) {
            _ur_inv_2_sigma_squared = RealType(0.5) / (_sigma * _sigma);
        }
    }

    // True if the truncation range is in the left tail; we handle this by pretending it's in the
    // right tail, then flipping the final result.  Used by HALFNORMAL and EXPONENTIAL.
    bool _left_tail;

    // Use a union because we won't need both of these at once, but being able to use descriptive
    // variable names is nice.
    union {
        // For _method == UNIFORM, this is the constant term in the exp (before division by 2 sigma^2).
        // For a truncation range including the mean, this is 0; otherwise this is (a - mu)^2, where a
        // is the truncation point closer to the mean.
        RealType _ur_shift;

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
