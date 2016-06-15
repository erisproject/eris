#pragma once

namespace eris { namespace random { namespace detail {

// Constants for the truncated normal algorithm.  All of these values depend on the specific CPU,
// architecture, compiler, etc., so we only try to provide ballpark figures here that should work
// reasonably well.
template<class RealType>
struct truncnorm_threshold {
    /** The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use exponential
     * rejection using the approximation:
     *
     *     left =~ 0.5 * (left + sqrt(left^2 + 4*sigma^2))
     *
     * The right-hand-side value is the optimal (in terms of maximizing the acceptance rate of
     * draws), but is relatively expensive, so that above this threshold, the expected cost of the
     * extra draws required is less than the computational cost of the above expression.  Below this
     * threshold, the calculation is worthwhile.
     */
    static constexpr RealType er_approximate_above = RealType(1.33);

    /** The value of the closer-to-mean limit, expressed as a standardized normal value (i.e.
     * \f$\alpha = \frac{\ell - \mu} / \sigma\f$) above which it is more efficient to use exponential
     * rejection sampling instead of half normal rejection sampling.  This value applies when the
     * truncation range is contained entirely within one side of the distribution.  (Note, however,
     * that on either side of this condition, we may still resort to uniform rejection sampling if b
     * is too close to a).
     */
    static constexpr RealType hr_below_er_above = RealType(0.56);

    /** The value of `a*(b-a)` below which to prefer UR to ER when considering 2-sided truncation
     * draws that are contained entirely within one tail of the distribution.
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
    static constexpr RealType prefer_ur_multiplier = RealType(0.24);

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

    /** This is the linear approximation of the threshold value of (right-left) above which we
     * prefer halfnormal rejection sampling and below which we prefer uniform rejection sampling.
     */
    static constexpr RealType ur_hr_threshold(const RealType &left, const RealType &sigma) {
        return RealType(0.36) * sigma + RealType(0.44) * left;
    }
};

} } } // namespace eris::random::detail
