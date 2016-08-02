#pragma once
#include <eris/learning/BayesianLinear.hpp>
#include <Eigen/Core>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <limits>
#include <string>
#include <utility>

namespace eris { namespace learning {

/** Extension to the base BayesianLinear class that supports prior restrictions on parameters via Monte
 * Carlo integration that rejects restricted draws.
 *
 * Single variable restrictions can be specified via the lowerBound(), upperBound(), and restrict()
 * methods while more general linear restrictions can be specified by calling addRestriction().
 *
 * This implementation can draw by either performing Gibbs sampling or by using rejection sampling
 * (calling BayesianLinear::draw() repeatedly until the base draw() method returns an admissible value).  By
 * default, it attempts rejection sampling first and switches to Gibbs sampling if rejection
 * sampling fails.
 *
 * Like BayesianLinear, this imposes a natural conjugate (Normal-gamma) prior on the model.
 *
 * Warning: no checking is performed for the viability of restrictions when they are added:
 * specifying impossible-to-satisfy restrictions (for example, both \f$\beta_1 + 2 \beta_2 \geq 4\f$
 * and \f$2\beta_1 + 4\beta_2 \leq 2\f$) upper and lower bounds (e.g. \f$\beta_2 \geq 3\f$ and
 * \f$\beta_2 \leq 2\f$) will result in drawRejection() always failing, and an error from
 * drawGibbs().
 */
class BayesianLinearRestricted : public BayesianLinear {
    public:
        /// Default constructor
        BayesianLinearRestricted() = default;
        /// Default copy constructor
        BayesianLinearRestricted(const BayesianLinearRestricted&) = default;
        /// Default copy assignment operator
        BayesianLinearRestricted& operator=(const BayesianLinearRestricted&) = default;
        /// Default move constructor
        BayesianLinearRestricted(BayesianLinearRestricted&&) = default;
        /// Default move assignment
        BayesianLinearRestricted& operator=(BayesianLinearRestricted&&) = default;
        /// Other constructors inherited from BayesianLinear
        using BayesianLinear::BayesianLinear;

    protected:
        // forward declaration
        class RestrictionProxy;
        class RestrictionIneqProxy;

    public:

        /** Returns a proxy object that allows assigning coefficient upper bounds.  Element `i` is
         * the upper bound for `beta[i]`, for `i` values between 0 and K-1.
         *
         * For example, to add the restriction \f$\beta_2 \leq 5\f$:
         * 
         *     model.upperBound(2) = 5.0;
         *
         * Warning: no checking is performed for the viability of specified bounds: specifying
         * conflicting upper and lower bounds (e.g. \f$\beta_2 \geq 3\f$ and \f$\beta_2 \leq 2\f$)
         * will result in drawRejection() never succeeding, and an error from drawGibbs().
         */
        RestrictionProxy upperBound(size_t k);

        /** Const version of above. */
        const RestrictionProxy upperBound(size_t k) const;

        /** Returns a proxy object that allows assigning coefficient lower bounds.  Element `i` is
         * the lower bound for `beta[i]`, for `i` values between 0 and K-1.
         *
         * For example, to add the restriction \f$\beta_2 \geq 0\f$:
         * 
         *     model.lowerBound(2) = 0.0;
         *
         * Warning: no checking is performed for the viability of specified bounds: specifying
         * conflicting upper and lower bounds (e.g. \f$\beta_2 \geq 3\f$ and \f$\beta_2 \leq 2\f$)
         * will result in drawRejection() never succeeding, and an error from drawGibbs().
         */
        RestrictionProxy lowerBound(size_t k);

        /** Const version of above. */
        const RestrictionProxy lowerBound(size_t k) const;

        /** Returns a proxy object that allows adding both upper and lower bounds for an individual
         * parameter.  Element `i` is the restriction object for `beta[i]`, for `i` values between 0
         * and K-1.
         *
         * To add a restriction, use the <= or >= operator.
         *
         * For example, to add the restrictions \f$0 \leq \beta_2 \leq 5\f$:
         *
         *     model.restrict(2) >= 0;
         *     model.restrict(2) <= 5;
         *
         * The restriction object's <= and >= operators return the object itself, so you can chain
         * restrictions together, such as the following (equivalent to the above):
         *
         *     model.restrict(2) >= 0 <= 5;
         */
        RestrictionIneqProxy restrict(size_t k);

        /** Const version of above. */
        const RestrictionIneqProxy restrict(size_t k) const;

        /** Adds a restriction of the form \f$R\beta \leq r\f$, where \f$R\f$ is a 1-by-`K()` row
         * vector selecting \f$\beta\f$ elements.  For example, to add a \f$\beta_2 \in [-1, 3.5]\f$
         * restriction, any of the following two approaches can be used:
         *
         *     model.upperBound(2) = 3.5;
         *     model.lowerBound(2) = -1;
         *
         *     model.restrict(2) >= -1 <= 3.5;
         *
         *     Eigen::RowVectorXd R = Eigen::RowVectorXd::Zero(K());
         *     R[2] = 1;
         *     model.addRestriction(R, 3.5);
         *     R[2] = -1;
         *     model.addRestriction(R, 1);
         *
         * NB: to add a greater-than-or-equal restriction, convert the restriction to a
         * less-than-or-equal one (by negating the `R` and `r` values, as in the above example).
         *
         * This method allows arbitrary linear relationships between the variables.  For example,
         * the following adds the restriction \f$\beta_1 - 3\beta_3 \geq 2.5\f$:
         *
         *     Eigen::RowVectorXd R = Eigen::RowVectorXd::Zero(K());
         *     R[1] = -1; R[3] = 3;
         *     model.addRestriction(R, -2.5);
         *
         * Example adding the restriction \f$\beta_1 \leq 10\beta_5\f$ to a model with 7
         * parameters, using Eigen's comma initialization syntax:
         *
         *     Eigen::RowVectorXd R(7);
         *     R << 0, 1, 0, 0, 0, -10, 0;
         *     model.addRestriction(R, 0);
         *
         * \throws std::logic_error if R has a length other than `K()`.
         */
        void addRestriction(const Eigen::Ref<const Eigen::RowVectorXd> &R, double r);

        /** Adds a `RB >= r` restriction.  This is simply a shortcut for calling
         *
         *     addRestriction(-R, -r)
         *
         * \sa addRestriction
         */
        void addRestrictionGE(const Eigen::Ref<const Eigen::RowVectorXd> &R, double r);

        /** Adds a set of linear restrictions of the form \f$R\beta \leq r\f$, where \f$R\f$ is a
         * l-by-`K()` matrix of parameter selection coefficients and \f$r\f$ is a l-by-1 vector of
         * value restrictions corresponding to the `l` restrictions specified in `R`.  The
         * restrictions must be satisfied for all `l` rows.
         *
         * This is equivalent to using addRestriction() l times, on each row of `R` and `r`.
         *
         * `R` and `r` must have the same number of rows.
         */
        void addRestrictions(const Eigen::Ref<const Eigen::MatrixXd> &R, const Eigen::Ref<const Eigen::VectorXd> &r);

        /** Adds a set of linear restrictions of the form \f$R\beta \geq r\f$.  This is simply a
         * shortcut for calling
         *
         *     addRestrictions(-R, -r);
         *
         * \sa addRestrictions
         */
        void addRestrictionsGE(const Eigen::Ref<const Eigen::MatrixXd> &R, const Eigen::Ref<const Eigen::VectorXd> &r);

        /// Returns the number of restrictions.  This is simply an alias for calling `R().rows()`.
        size_t numRestrictions() const { return R().rows(); }

        /** Clears all current model restrictions. */
        void clearRestrictions();

        /** Clears the restriction stored as row `r` of R(). */
        void removeRestriction(size_t r);

        /** The different draw modes supported by the `draw_mode` parameter and the `draw(mode)`
         * method.
         */
        enum class DrawMode { Auto, Gibbs, Rejection };

        /// The default draw mode used when calling draw() without an argument.
        DrawMode draw_mode = DrawMode::Auto;

        /** The last actual draw mode used, either DrawMode::Gibbs or DrawMode::Rejection.
         * DrawMode::Auto is only set if no draws have been performed.  If DrawMode::Auto was used
         * for the last draw, this value can be used to detect which type of draw actually took
         * place.
         */
        DrawMode last_draw_mode{DrawMode::Auto};

        /** Performs a draw according to the current value of `draw_mode`; valid values are:
         *
         *     - DrawMode::Gibbs - for gibbs sampling.
         *     - DrawMode::Rejection - for rejection sampling.
         *     - DrawMode::Auto - try rejection sampling first, switch to Gibbs sampling if required
         *
         * When using DrawMode::Auto, rejection sampling is tried first by calling drawRejection()
         * with an appropriate maximum number of attempts (see below).  If the rejection sampling
         * algorithm cannot find an admissable value, gibbs sampling is used instead.  Subsequent
         * calls to draw() will continue to use gibbs sampling.
         *
         * The maximum number of rejection attempts is the number required to reach the larger of a
         * cumulative total (including past attempt) of at least `draw_auto_min_rejection` (50 by
         * default) total rejection draws (whether successful or failed); or the number of failed
         * draws required for the success rate to drop below `draw_auto_min_success_rate` (0.2 by
         * default).
         *
         * For example, with the default values, if there have been 40 successful and 100 failed
         * past draw attempts, up to 61 rejection draw attempts will be made (because after 61
         * sequential failures, the success rate will have dropped to 40/201 < 0.2).  If there is 1
         * successful and 10 failed past draws, 39 rejection draw attempts will be made (to hit the
         * draw_auto_min_rejection minimum attempts).
         *
         * Once the conditions are met, they will also be met for future calls (unless the threshold
         * variables are changed), and so once auto-mode starts using gibbs, it will continue using
         * gibbs for subsequent draws without attempt further rejection sampling.
         *
         * \sa draw_mode
         */
        const Eigen::VectorXd& draw() override;

        /** Draws a set of parameter values, incorporating the restrictions using Gibbs sampling
         * rather than rejection sampling (as done by drawRejection()).  Returns a const reference
         * to the `k+1` length vector where elements `0` through `k-1` are the \f$\beta\f$ values
         * and element `k` is the \f$h^{-1} = sigma^2\f$ draw.
         *
         * The Gibbs sampler draw uses truncated normal drawing loosely based on the description in
         * Rodriguez-Yam, Davis, and Scharf (2004), "Efficient Gibbs Sampling of Truncated
         * Multivariate Normal with Application to Constrained BayesianLinear Regression," with
         * modifications as described below to draw from a truncated multivariate t instead.
         *
         * The behaviour of drawGibbs() is affected by three parameters:
         * - `draw_gibbs_burnin` controls the number of draws to discard before returning the first
         *   drawGibbs() call.  Any previous burn-in, thinning, and successful draws (as determined
         *   by `draw_gibbs_success + draw_gibbs_discards`) are considered to satisfy the burn-in
         *   requirement.
         * - `draw_gibbs_thinning` controls how many (internal) draws to discard before returning a
         *   drawGibbs() draws--every `draw_gibbs_thinning`th draw is used (i.e. thinning=1 uses no
         *   thinning).  The thinning does not apply to burn-in draws.
         *
         * \par Implementation details: initial value
         *
         * Rodriguez-Yam et al.\ do not discuss the initial draw, but this seems a large omission
         * from their algorithm: unlike Geweke's earlier work (which is their main benchmark for
         * comparison), their algorithm can fail for points outside the initial truncated region.
         * (Geweke's algorithm, in contrast, does not, though it may wander around outside the
         * truncated region for many initial draws, and may thus require a larger burn-in period).
         * Specifically, in iteration \f$t\f$ for parameter \f$j\f$ in the Rodriguez-Yam et al.\
         * algorithm, one must identify the lower and upper-bounds for parameter \f$j\f$ as required
         * by the model restrictions conditional on parameters \f$1, \ldots, j-1\f$ from the current
         * iteration and \f$j+1, \ldots, k\f$ from the previous iteration.  If there are linear
         * restrictions involving multiple parameters that are violated by the previous draw (or
         * starting point), it can easily be the case that the upper bound on parameter \f$j\f$ is
         * below the lower bound.
         *
         * For example, consider the restrictions \f$\beta_1 \geq 0\f$ and \f$\beta_1 \leq
         * \beta_2\f$, with starting location \f$\beta = [-0.5, -1]^\top\f$.  From the first
         * restriction, we get a lower limit for \f$\beta_1^{(1)}\f$ of 0; applying the second
         * restriction \f$\beta_1^{(1)} \leq \beta_2^{(0)} = -1\f$, in other words, an upper limit
         * of -1.  The algorithm of Rodriguez-Yam et al.\ then requires a draw of
         * \f$\beta_1^{(1)}\f$ truncated to satisfy both \f$\beta_1 \leq -1\f$ and \f$\beta_1 \geq
         * 0\f$, which is clearly impossible.
         *
         * To solve the above problem, we thus need to be certain that the constraints are
         * satisfied.  It doesn't particularly matter how they are satisfied: because of the burn-in
         * period of the sampler, the effects of a initial value far off in the tail of the
         * distribution aren't particularly relevant.  Thus, to choose an initial point (if one
         * hasn't been explicitly given by a call to gibbsInitialize()) we simply take a draw from
         * the unrestricted distribution, then pass it to gibbsInitialize() to manipulate that point
         * into the restriction-satisfying parameter space.  If that fails (specifically, if a point
         * satisfying the restrictions hasn't been found after \f$10k\f$ steps) another point is
         * drawn and passed to gibbsInitialize().  This is repeated for a total of 10 attempts,
         * after which we give up with an error (as this likely indicates that the model has some
         * sort of impossible-to-satisfy prior restrictions).
         *
         * Once an acceptable initial position is found, the algorithm followed is essentially the
         * same as that of Rodriguez-Yam et al.\ with modifications for drawing from a multivariate
         * t instead of multivariate normal, and is described in full below.
         *
         * \par Implementation details: drawing beta values
         *
         * In general, a non-truncated multivariate \f$t(\mu, \Sigma, \nu)\f$ can easily be formed
         * by adding the mean to \f$\sqrt{\nu / (s^2 \chi^2_{\nu})}\f$ times a draw from the
         * (non-truncated) multivariate \f$N(\mathbf{0}, \mathbf{\Sigma})\f$ distribution (which
         * itself is easily drawn as \f$\mu + \mathbf{L} \mathbf{z}\f$, where \f$\mathbf{L}
         * \mathbf{L}^\top = \mathbf{\Sigma}\f$ (i.e.\ the Cholesky decomposition of
         * \f$\mathbf{\Sigma}\f$) and \f$z\f$ is a vector of \f$k\f$ independent standard normals).
         *
         * Rejection sampling (as performed by the drawRejection() method) can then use the above to
         * draw from the unconditional distribution and discarding any draws that violate the prior
         * restrictions.  This is impractical, however, when there are restrictions that eliminate a
         * large portion of draws from the unrestricted draw space.
         *
         * Rodriguez-Yam et al.\ address this issue for drawing a truncated multivariate normal,
         * \f$\mathbf{X} \sim N_T(\mathbf{\mu}, \sigma^2\mathbf{\Sigma})\f$, with truncation area
         * \f$T\f$ being the values of \f$X\f$ satisfying \f$\mathbf{R}\mathbf{x} \leq
         * \mathbf{r}\f$.  To do this, they use Gibbs sampling, but first reparameterize the problem
         * to draw from the (independent) multivariate normal \f$Z \sim N_S(\mathbf{A}\mathbf{\mu},
         * \sigma^2\mathbf{I_k})\f$.  \f$\mathbf{A}\f$ here is any matrix satisfying
         * \f$\mathbf{A}\mathbf{\Sigma}\mathbf{A}^\top = \mathbf{I}\f$, where an obvious choice of
         * \f$\mathbf{A}\f$ is the inverse of the Cholesky decomposition of \f$mathbf{\Sigma}\f$, that
         * is, \f$\mathbf{L}^{-1}\f$ where \f$\mathbf{L}\mathbf{L}^\top = \mathbf{\Sigma}\f$.  The
         * truncation region is, of course, the same, but reexpressed in terms of \f$\mathbf{z}\f$
         * as points satisfying \f$\mathbf{D}\mathbf{z} \leq b\f$, where \f$\mathbf{D} =
         * \mathbf{R}\mathbf{A}^{-1}\f$.
         *
         * The problem has thus been reformed to one in which independent normal draws can be used
         * to draw \f$\beta_j\f$ values one-by-one, conditional on the most recent draw of \f$h
         * \equiv sigma^{-2}\f$ (discussed below) and the other values of \f$\beta{-j}\f$.  The
         * prior restrictions immediately imply an admissable range for \f$\beta_j\f$: it is whatever
         * range of values will satisfy \f$\mathbf{R}\beta^* \leq \mathbf{r}\f$, where \f$\beta^*\f$
         * is made up of the current iteration's draws for \f$\beta_{i<j}\f$ and the previous
         * iteration's draws for \f$\beta_{i>j}\f$.  Thus each row of \f$\mathbf{R}\f$ with a
         * non-zero value in the \f$j\f$th column defines either an upper or lower bound for
         * \f$\beta_j\f$ (and implicitly also a bound on \f$z_j\f$); from these bounds, the most
         * restrictive upper and lower bounds for \f$z_j\f$ are chosen (\f$\pm \infty\f$ is used for
         * non-existant bounds).  The issue described above---where the algorithm must begin inside the
         * unrestricted space---should be apparent: if, for example, \f$\beta_1\f$ and \f$\beta_2\f$
         * are independent and a \f$\beta_2 > 0\f$ restriction is violated at the current position,
         * there are no range of values for \f$z_1\f$ that can avoid violating the constraint.
         *
         * Once the range on \f$z_j\f$ is established, it is simply a matter of drawing from the
         * truncated univariate normal \f$N(0,\sigma^2)\f$ with truncation region \f$\left[z_{lower},
         * z_{upper}\right]\f$.  This implementation slightly alters the approach of Rodriguez-Yam
         * et al.\ to let \f$Z\f$ be a vector of independent standard normals (instead of
         * independent \f$N(0, \sigma^2)\f$), for slightly easier computation (unlike Rodriguez-Yam
         * et al.\, the \f$\sigma^2\f$ multiple changes from Gibbs iteration to the next as new \f$h
         * = \sigma^{-2}\f$ values are drawn conditional on the \f$beta\f$s).
         *
         * \par Implementation details: drawing h values
         *
         * Values of \f$h\f$ are actually drawn before the \f$\beta\f$ values in each Gibbs
         * iteration, but it is more helpful to have described the \f$\beta\f$ draw procedure first.
         *
         * Rodriguez-Yam et al.\ limit their paper to drawing truncated multivariate normals, but
         * what we actually need is to draw multivariate \f$t\f$.  As mentioned earlier, this
         * requires drawing a \f$h = \sigma^{-2}\f$ value conditional on the beta.  The procedure
         * used for this is fundamentally the same as the above: we determine the range of values of
         * W such that \f$\sqrt{n/W} \overline{s} \overline{\mathbf{L}} \mathbf{Z}\f$ (which is just
         * \f$\beta\f$, but recalculated with a new \f$W\f$ value) does not violate any
         * restrictions.  It is then a relatively simple matter to draw \f$W ~
         * \chi^2_{\overline{\nu}}\f$ restricted to the range of W that does not violate the prior
         * constraints.
         *
         */
        virtual const Eigen::VectorXd& drawGibbs();

        /** Initialize the Gibbs sampler with the given initial values of beta, adjusting it, if
         * necessary, to satisfy the model constraints.  If drawGibbs() has previously been called,
         * calling this method will cause the next drawGibbs() call to perform a burn-in.  If the
         * initial value satisfies all the restrictions, it is used as is.  Otherwise, the starting
         * point is adjusted with respect to the violated constraints repeatedly until an admissable
         * point is found, or the maximum number of adjustments is reached.
         *
         * The specific adjustment is as follows:
         * - Let \f$v \equiv R\beta - r\f$ be the vector of violation amounts, and \f$v_+ \equiv
         *   \sum_{i=1}{l}I(v_i > 0)v_i\f$, i.e.\ the sum of the strictly positive values of
         *   \f$v\f$.
         * - Select a random row (with equal probability of each) from the set of rows of \f$v\f$
         *   that are strictly positive (i.e.  from the violated constraints).
         * - Adjust the current position by moving directly towards the nearest point on the
         *   constraint in the transformed orthogonal space, and move a distance of 1.5 times the
         *   distance to that nearest point.  In other words, we "overshoot" the constraint boundary
         *   by 50%.
         * - Repeat until either all the constraints are satisfied.  If the constraints still
         *   haven't been satisfied after 100 iterations, abort with an exception.
         *
         * Notes:
         * - The reason for the overshoot is to attempt to avoid a condition where two constraints
         *   are at an acute angle to each other; depending on the initial violation (in particular,
         *   in any initial position where jumping to the boundary of either constraint will violate
         *   the other constraint), we could end up "stairstepping" towards the constraint
         *   intersection point, but require (mathematically) an infinite number of steps to reach
         *   it; with numerical imprecision we might or might not reach it at all.  The overshoot is
         *   designed to help alleviate this.
         * - This algorithm is invariant to scaling in the restriction specification (that is,
         *   restriction specifications of \f$\beta_1 + \beta_2 \leq 2\f$ and \f$5\beta_1 + 5\beta_2
         *   \leq 10\f$ are treated identically).
         * - This algorithm specifically does not select violated rows based on the size of the
         *   violation or the distance from current position to the violated boundary, because there
         *   is no particular reason to think that cross-row restrictions are comparable.  For
         *   example, suppose a model has restriction \f$\beta_1 \geq 1\f$ with data \f$X_1\f$ and
         *   another model uses the rescaled data \f$10X_1\f$ with restriction \f$\gamma_1 \geq
         *   10\f$.  The size of the violation from the same (relative) draw, and the Euclidean
         *   distance from the inadmissable point to the restriction boundary, will have been scaled
         *   by 10; thus depending on either in the selection of constraints to attempt to satisfy
         *   would result in model reparameterizations fundamentally changing the algorithm, which
         *   seems an undesirable characteristic.
         * - One potential solution to the above would be to normalize the parameters.  This,
         *   however, would require that the number of parameters involved in restrictions equals
         *   the number of restrictions (so that the restricted values can be normalized by the same
         *   matrix that normalizes the restricted \f$\beta\f$'s), but that sort of limitation isn't
         *   otherwise required by the Gibbs algorithm used in this class.
         *
         * \param initial the initial value of \f$\beta\f$.  May also include an extra
         * \f$\sigma^2\f$ value, which will be ignored.
         * \param max_tries the maximum tries to massage the initial point into unrestricted
         * parameter space.  Defaults to 100.
         * \throws constraint_failure if the constraint cannot be satisfied after `max_tries`
         * adjustments.
         * \throws logic_error if called with a vector with size other than `K` or `K+1`
         */
        void gibbsInitialize(const Eigen::Ref<const Eigen::VectorXd> &initial, unsigned long max_tries = 100);

        /** Exception class used to indicate that one or more constraints couldn't be satisfied.
         */
        class constraint_failure : public draw_failure {
            public:
                /** Constructor.
                 * \param what the exception message.
                 */
                constraint_failure(const std::string &what) : draw_failure(what) {}
        };

        /** This method attempts to draw the `K+1` \f$(\beta,h^{-1})\f$ values as if no restrictions
         * are in place (by calling BayesianLinear::draw()), then repeats the draw as long as the drawn
         * values violate one or more of the models restrictions (or `draw_discards_max`
         * unsuccessful draws occur).
         *
         * This is more efficient than drawGibbs() when the likelihood of restricted values being
         * drawn is very small; it is usually recommended to call `draw()` instead, which tries this
         * and switches to Gibbs if the acceptance rate is low.
         *
         * After calling, whether returning or throwing a draw_failure exception,
         * `draw_rejection_discards`, `draw_rejection_discards_cumulative`, and
         * `draw_rejection_success_cumulative` will have been updated to reflect the draw
         * statistics.
         *
         * \param max_discards if specified and non-negative, the maximum number of failed draws
         * before aborting (by throwing an exception).  The default value, -1, uses
         * `draw_rejection_max_discards`.
         *
         * \throws BayesianLinear::draw_failure exception if, due to the imposed restrictions, at least
         * `draw_discards_max` sequential draws are discarded without finding a single admissible
         * draw.
         */
        virtual const Eigen::VectorXd& drawRejection(long max_discards = -1);

        // These are const from the public point of view; privately we adjust them (via a
        // const_cast)
        int
            draw_rejection_discards_last{0}, ///< Tracks the number of inadmissable draws by the most recent call to drawRejection()
            draw_rejection_success{0}, ///< The cumulative number of successful rejection draws
            draw_rejection_discards{0}, ///< The cumulative number of inadmissable rejection draws
            draw_gibbs_success{0}, ///< The cumulative number of gibbs draws obtained, not including burn-in and thinning draws
            draw_gibbs_discards{0}, ///< The cumulative number of discarded (for burn-in or thinning) gibbs draws
            draw_rejection_max_discards{50}, ///< The maximum number of inadmissable draws for a single rejection draw before aborting
            draw_auto_min_rejection{50}, ///< The minimum number of Auto-mode rejection attempts before consider the success rate
            draw_gibbs_burnin{100}, ///< The number of burn-in draws for the first Gibbs sampler draw
            draw_gibbs_thinning{2}; ///< drawGibbs() uses every `draw_gibbs_thinning`th sample (1 = use every draw)
        double draw_auto_min_success_rate{0.2}; ///< The minimum draw success rate below which we switch to Gibbs sampling

        /// Accesses the restriction coefficient selection matrix (the \f$R\f$ in \f$R\beta <= r\f$).
        const Eigen::MatrixXd& R() const { return restrict_select_; }

        /// Accesses the restriction value vector (the \f$r\f$ in \f$R\beta <= r\f$).
        const Eigen::VectorXd& r() const { return restrict_values_; }

        /** Overloaded to append the restrictions after the regular BayesianLinear details.
         */
        virtual operator std::string() const override;

        /** The display name of the model to use when printing it.  Defaults to "BayesianLinearRestricted"
         * but subclasses should override.
         */
        virtual std::string display_name() const override;

    protected:
        /// Creates a BayesianLinearRestricted from a BayesianLinear rvalue
        BayesianLinearRestricted(BayesianLinear &&move) : BayesianLinear(std::move(move)) {}

        /** Resets any drawn values and draw-related variables.  Called automatically when adding a
         * restriction, in addition to the `BayesianLinear` cases (updating and weakening).
         */
        virtual void reset() override;

        /** Proxy object that converts assignments into restriction rows on the associated
         * BayesianLinearRestricted model.
         *
         * \sa upperBound()
         * \sa lowerBound()
         */
        class RestrictionProxy final {
            public:
                /** Adds a restriction on the referenced parameter.
                 */
                RestrictionProxy& operator=(double r);
                /** Returns true if there is a restriction of the appropriate type (upper- or
                 * lower-bound) on the given parameter.  This method works whether the restriction
                 * was added via a RestrictionProxy object, or as a single-parameter restriction
                 * given to addRestriction() (or variants).
                 *
                 * Specifically this looks for any restriction with a negative (for lower bounds) or
                 * positive (for upper bounds) selector for the requested coefficient and 0 for all
                 * other coefficient selectors.
                 */
                bool restricted() const;
                /** Returns the appropriate value restriction, if there is a single-parameter
                 * restriction of the given type.  If there is no such restriction, returns a
                 * quiet_NaN.  If there are multiple restrictions, the most binding restriction is
                 * returned (that is, 4 is returned if both `>= 3` and `>= 4` lower-bound
                 * restrictions are found).
                 *
                 * Note that restrictions added with non-unitary coefficients are handled properly;
                 * that is, if a `<=` restriction on a 4-parameter model of R=(0, -2.5, 0, 0), r=1
                 * is added, the returned lower bound restriction will be -0.4.
                 */
                operator double() const;
            private:
                /** Creates a new RestrictionProxy object that, when assigned to, adds an upper or
                 * lower-bound restriction on beta[`k`].
                 *
                 * \param the BayesianLinearRestricted object where the restriction will be added;
                 * \param k the parameter index
                 * \param upper true if this is an upper bound, false if a lower bound
                 */
                RestrictionProxy(BayesianLinearRestricted &lr, size_t k, bool upper);
                friend class BayesianLinearRestricted;
                BayesianLinearRestricted &lr_;
                const size_t k_;
                const bool upper_;
        };

        /** Proxy object that converts <= and >= inequality operators into new restriction rows on
         * the associated BayesianLinearRestricted model.
         *
         * \sa restrict()
         */
        class RestrictionIneqProxy final {
            public:
                /** Adds an upper-bound restriction on the referenced parameter.
                 */
                RestrictionIneqProxy& operator<=(double r);
                /** Adds a lower-bound restriction on the referenced parameter.
                 */
                RestrictionIneqProxy& operator>=(double r);
                /** Returns true if there is an upper-bound restriction on the associated parameter.
                 * This method works whether the restriction was added via a proxy object or as a
                 * single-parameter restriction given to addRestriction() (or variants).
                 *
                 * Specifically this looks for any restriction with a positive selector for the
                 * requested coefficient and 0 for all other coefficient selectors.
                 */
                bool hasUpperBound() const;
                /** Returns the most-binding upper-bound restriction on the associated parameter.
                 * If the parameter has no upper-bound restrictions at all, returns NaN.
                 */
                double upperBound() const;
                /** Returns true if there is an lower-bound restriction on the associated parameter.
                 * This method works whether the restriction was added via a proxy object or as a
                 * single-parameter restriction given to addRestriction() (or variants).
                 *
                 * Specifically this looks for any restriction with a negative selector for the
                 * requested coefficient and 0 for all other coefficient selectors.
                 */
                bool hasLowerBound() const;
                /** Returns the most-binding lower-bound restriction on the associated parameter.
                 * If the parameter has no lower-bound restrictions at all, returns NaN.
                 */
                double lowerBound() const;
            private:
                /** Creates a new RestrictionProxy object that, when assigned to, adds an upper or
                 * lower-bound restriction on beta[`k`].
                 *
                 * \param the BayesianLinearRestricted object where the restriction will be added;
                 * \param k the parameter index
                 */
                RestrictionIneqProxy(BayesianLinearRestricted &lr, size_t k);
                friend class BayesianLinearRestricted;
                BayesianLinearRestricted &lr_;
                const size_t k_;
        };

        /** Returns true if there is a single-parameter, upper- or lower-bound restriction on
         * `beta[k]`.  Note that this ignores any multi-parameter restriction involving `beta[k]`.
         *
         * \sa RestrictionProxy::restricted()
         */
        bool hasRestriction(size_t k, bool upper) const;

        /** Returns the most-binding single-parameter, upper- or lower-bound restriction on
         * `beta[k]`, or NaN if `beta[k]` has no such restriction.  Note that this ignores any
         * multi-parameter restriction involving `beta[k]`.
         *
         * \sa RestrictionProxy::operator double()
         */
        double getRestriction(size_t k, bool upper) const;

        /** Stores the coefficient selection matrix for arbitrary linear restrictions passed to
         * addRestriction() or addRestrictions().
         */
        Eigen::MatrixXd restrict_select_;
        /** Stores the value restrictions for arbitrary linear restrictions passed to
         * addRestriction() or addRestrictions().
         */
        Eigen::VectorXd restrict_values_;

    private:

        // Takes a z-space vector, returns the associated beta-space vector.  Note that the
        // associated beta value is this vector *plus* `beta()`.  Note also that the value returned
        // here incorporates s2_; a further sigma multiplier can provided when necessary (e.g.
        // when drawing a t distribution), which scales the result by the given value.
        Eigen::VectorXd toBetaVector(const Eigen::Ref<const Eigen::VectorXd> &z, double sigma_multiplier = 1.0) const;

        // Takes an initial beta value, returns the same converted into z-space.  Note that the
        // input value should be the actual beta value, *not* the vector from beta() to beta.
        // This should only be used to convert an initial value into an initial z value; after that,
        // the z value itself should be used for subsequent gibbs draws.
        Eigen::VectorXd toInitialZ(const Eigen::Ref<const Eigen::VectorXd> &initial_beta) const;

        // Takes a z vector, returns the associated restriction slackness vector (i.e.  r - R*beta),
        // where negative values indicate violated restrictions.
        // sigma_multiplier is an extra value to multiply the rootSigma matrix by (note that
        // rootSigma already incorporates s2_).
        Eigen::VectorXd restrictionViolations(const Eigen::Ref<const Eigen::VectorXd> &z, double sigma_multiplier = 1.0) const;


        mutable std::shared_ptr<Eigen::VectorXd> r_minus_R_beta_center_;
        // Calculates (if not already cached) the value of `r - R()*beta()`, i.e. the restriction
        // slackness at the beta() posterior parameter.
        const Eigen::VectorXd& rMinusRBeta() const;

        // The cache for the following method (the method still has to multiply by sigma_multiplier,
        // but doesn't have to repeat the matrix multiplication).
        mutable std::shared_ptr<Eigen::MatrixXd> to_net_restr_unscaled_;
        // Calculates the matrix that premultiplies `z` into net restriction values.  That is, this
        // matrix times z is equivalent to R() * toBetaVector(z), and so
        // `rMinusRBeta() - netRestrictionMatrix() * z` will equal will equal `r - R()*beta`, where
        // beta equals beta() plus the beta vector associated with z.  Note that this matrix is
        // *not* scaled by a sigma_multiplier: when a non-unitary sigma_multiplier is needed,
        // multiply the returned value by the multiplier.
        const Eigen::MatrixXd& netRestrictionMatrix() const;

        /* Returns the bounds on a sigma multiplier for the given z draw that doesn't violate any
         * restrictions. Lower bound is .first, upper bound is .second.
         *
         * range.first will be non-negative--a value of 0 will occur whenever beta() does not
         * violate any restrictions.
         * range.second can be negative, or less than range.first: both indicate that there are no
         * admissable positive multipliers that can avoid violating at least one restriction, and in
         * general means that the current z already violates restrictions.
         */
        std::pair<double, double> sigmaMultiplierRange(const Eigen::VectorXd &z) const;

        // z ~ restricted N(0, I)
        std::shared_ptr<Eigen::VectorXd> gibbs_last_z_;

        double chisq_n_median_ = std::numeric_limits<double>::quiet_NaN();

};

}}
