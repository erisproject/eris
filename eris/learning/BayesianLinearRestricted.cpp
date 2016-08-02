#include <eris/learning/BayesianLinearRestricted.hpp>
#include <eris/learning/BayesianLinear.hpp>
#include <eris/random/rng.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <eris/random/truncated_distribution.hpp>
#include <cmath>
#include <boost/random/chi_squared_distribution.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/math/distributions/complement.hpp>
#include <stdexcept>
#include <vector>
#include <set>
#include <sstream>
#include <type_traits>

using namespace Eigen;

namespace eris { namespace learning {

BayesianLinearRestricted::RestrictionProxy BayesianLinearRestricted::lowerBound(size_t k) {
    return RestrictionProxy(*this, k, false);
}
const BayesianLinearRestricted::RestrictionProxy BayesianLinearRestricted::lowerBound(size_t k) const {
    return RestrictionProxy(const_cast<BayesianLinearRestricted&>(*this), k, false);
}
BayesianLinearRestricted::RestrictionProxy BayesianLinearRestricted::upperBound(size_t k) {
    return RestrictionProxy(*this, k, true);
}
const BayesianLinearRestricted::RestrictionProxy BayesianLinearRestricted::upperBound(size_t k) const {
    return RestrictionProxy(const_cast<BayesianLinearRestricted&>(*this), k, true);
}
BayesianLinearRestricted::RestrictionIneqProxy BayesianLinearRestricted::restrict(size_t k) {
    return RestrictionIneqProxy(*this, k);
}
const BayesianLinearRestricted::RestrictionIneqProxy BayesianLinearRestricted::restrict(size_t k) const {
    return RestrictionIneqProxy(const_cast<BayesianLinearRestricted&>(*this), k);
}

void BayesianLinearRestricted::addRestriction(const Ref<const RowVectorXd> &R, double r) {
    if (R.size() != K_) throw std::logic_error("Unable to add linear restriction: R does not have size K");
    restrict_select_.conservativeResize(restrict_select_.rows() + 1, K_);
    restrict_values_.conservativeResize(restrict_values_.rows() + 1);
    restrict_select_.bottomRows(1) = R;
    restrict_values_[restrict_values_.rows()-1] = r;
    reset();
}

void BayesianLinearRestricted::addRestrictionGE(const Ref<const RowVectorXd> &R, double r) {
    return addRestriction(-R, -r);
}

void BayesianLinearRestricted::addRestrictions(const Ref<const MatrixXd> &R, const Ref<const VectorXd> &r) {
    if (R.cols() != K_) throw std::logic_error("Unable to add linear restrictions: R does not have K columns");
    auto num_restr = R.rows();
    if (num_restr != r.size()) throw std::logic_error("Unable to add linear restrictions: different number of rows in R and r");
    restrict_select_.conservativeResize(restrict_select_.rows() + num_restr, K_);
    restrict_values_.conservativeResize(restrict_values_.rows() + num_restr);
    restrict_select_.bottomRows(num_restr) = R;
    restrict_values_.tail(num_restr) = r;
    reset();
}

void BayesianLinearRestricted::addRestrictionsGE(const Ref<const MatrixXd> &R, const Ref<const VectorXd> &r) {
    return addRestrictions(-R, -r);
}

void BayesianLinearRestricted::clearRestrictions() {
    restrict_select_.resize(0, K_);
    restrict_values_.resize(0);
    reset();
}

void BayesianLinearRestricted::removeRestriction(size_t r) {
    size_t rows = restrict_select_.rows();
    if (r >= rows) throw std::logic_error("BayesianLinearRestricted::removeRestriction(): invalid restriction row `" + std::to_string(r) + "' given");

    size_t trailing = rows - 1 - r;
    if (trailing > 0) {
        // There are rows after the one to remove, so we have to copy them forward
        restrict_select_.middleRows(r, trailing) = restrict_select_.bottomRows(trailing).eval(); // eval to avoid aliasing
        restrict_values_.segment(r, trailing) = restrict_values_.tail(trailing).eval();
    }

    // Now resize away the extra row (either the already-copied last row, or the removed row if
    // there were no rows following it).
    restrict_select_.conservativeResize(rows-1, NoChange);
    restrict_values_.conservativeResize(rows-1);
    reset();
}

void BayesianLinearRestricted::reset() {
    BayesianLinear::reset();
    draw_rejection_discards_last = 0;
    draw_rejection_discards = 0;
    draw_rejection_success = 0;
    draw_gibbs_success = 0;
    draw_gibbs_discards = 0;
    r_minus_R_beta_center_.reset();
    gibbs_last_z_.reset();
    to_net_restr_unscaled_.reset();
    chisq_n_median_ = std::numeric_limits<double>::quiet_NaN();
}

const VectorXd& BayesianLinearRestricted::draw() {
    // If they explicitly want rejection draw, do it:
    if (draw_mode == DrawMode::Rejection)
        return drawRejection();

    DrawMode m = draw_mode;
    // In auto mode we might try rejection sampling first
    if (m == DrawMode::Auto) { // Need success rate < 0.2 to switch, with at least 50 samples.
        long draw_rej_samples = draw_rejection_success + draw_rejection_discards;
        double success_rate = draw_rejection_success / (double)draw_rej_samples;
        if (success_rate < draw_auto_min_success_rate and draw_rej_samples >= draw_auto_min_rejection) {
            // We saw too few successes before, so use Gibbs sampling
            m = DrawMode::Gibbs;
        }
        else {
            // Figure out how many sequential failed draws we'd need to hit the failure threshold,
            // then try up to that many times.
            long max_failures = std::max<long>(
                    std::ceil(draw_rejection_success / draw_auto_min_success_rate),
                    draw_auto_min_rejection)
                - draw_rej_samples;
            try {
                return drawRejection(max_failures);
            }
            catch (draw_failure&) {
                // Draw failure; switch to Gibbs
                m = DrawMode::Gibbs;
            }
        }
    }

    // Either Gibbs was requested explicitly, or Auto was asked for and rejection sampling failed
    // (either we tried it just now, or previous draws have too low a success rate), so use Gibbs
    return drawGibbs();
}

VectorXd BayesianLinearRestricted::toBetaVector(const Ref<const VectorXd> &z, double sigma_multiplier) const {
    VectorXd beta_vec = rootSigma() * z;
    if (sigma_multiplier != 1) beta_vec *= sigma_multiplier;
    return beta_vec;
}

VectorXd BayesianLinearRestricted::toInitialZ(const Ref<const VectorXd> &initial_beta) const {
    return rootSigma().solve(initial_beta - beta());
}

const VectorXd& BayesianLinearRestricted::rMinusRBeta() const {
    if (not r_minus_R_beta_center_) r_minus_R_beta_center_.reset(
            numRestrictions() == 0
            ? new VectorXd()
            : new VectorXd(r() - R() * beta()));
    return *r_minus_R_beta_center_;
}

const MatrixXd& BayesianLinearRestricted::netRestrictionMatrix() const {
    if (not to_net_restr_unscaled_) to_net_restr_unscaled_.reset(
            numRestrictions() == 0
            ? new MatrixXd(0, K_) // It's rather pointless to use drawGibbs() with no restrictions, but allow it for debugging purposes
            : new MatrixXd(R() * rootSigma()));
    return *to_net_restr_unscaled_;
}

VectorXd BayesianLinearRestricted::restrictionViolations(const Ref<const VectorXd> &z, double sigma_multiplier) const {
    // This rearranges to be r - Rbeta, and so has positive values for violated restrictions
    return rMinusRBeta() - netRestrictionMatrix() * z * sigma_multiplier;
}

void BayesianLinearRestricted::gibbsInitialize(const Ref<const VectorXd> &initial, unsigned long max_tries) {
    constexpr double overshoot = 1.5;

    if (initial.size() < K_ or initial.size() > K_+1) throw std::logic_error("BayesianLinearRestricted::gibbsInitialize() called with invalid initial vector (initial.size() != K())");

    auto &rng = random::rng();

    VectorXd z = toInitialZ(initial.head(K_));

    if (numRestrictions() == 0) {
        // No restrictions, nothing special to do!
        if (not gibbs_last_z_ or gibbs_last_z_->size() != K_) gibbs_last_z_.reset(new VectorXd(z));
        else *gibbs_last_z_ = z;
    }
    else {
        // Otherwise we'll start at the initial value and update
        std::vector<size_t> violations;
        violations.reserve(numRestrictions());

        for (unsigned long trial = 1; ; trial++) {
            violations.clear();
            VectorXd v = restrictionViolations(z);
            for (size_t i = 0; i < numRestrictions(); i++) {
                if (v[i] < 0) violations.push_back(i);
            }
            if (violations.size() == 0) break; // Restrictions satisfied!
            else if (trial >= max_tries) { // Too many tries: give up
                // Clear gibbs_last_ on failure (so that the next drawGibbs() won't try to use it)
                gibbs_last_z_.reset();
                throw constraint_failure("gibbsInitialize() couldn't find a way to satisfy the model constraints");
            }

            // Select a random constraint to fix:
            size_t fix = violations[boost::random::uniform_int_distribution<size_t>(0, violations.size()-1)(rng)];

            // Aim at the nearest point on the boundary and overshoot (by 50%):
            z += overshoot * v[fix] / netRestrictionMatrix().row(fix).squaredNorm() * netRestrictionMatrix().row(fix).transpose();
        }

        if (not gibbs_last_z_ or gibbs_last_z_->size() != K_) gibbs_last_z_.reset(new VectorXd(K_));
        *gibbs_last_z_ = z;
    }
}

const VectorXd& BayesianLinearRestricted::drawGibbs() {
    last_draw_mode = DrawMode::Gibbs;

    if (not gibbs_last_z_) {
        // If we don't have an initial value, draw an *untruncated* value and give it to
        // gibbsInitialize() to fix up.
        for (int trial = 1; ; trial++) {
            try { gibbsInitialize(BayesianLinear::draw(), 10*numRestrictions()); break; }
            catch (constraint_failure&) {
                if (trial >= 10) throw;
                // Otherwise try again
            }
        }

        // gibbs_last_*_ will be set up now (or else we threw an exception)
    }
    // Start with z from the last z draw (or the initial value drawn (and possibly fixed up) above)
    VectorXd z(*gibbs_last_z_);

    // The draw will eventually equal beta() + sigma_multiplier * toBetaVector(z).  sigma_multiplier
    // would be exactly 1 if drawing from a Normal; it's a value involving a chi^2 draw for our
    // desired t distribution (see below).
    double sigma_multiplier = 0;

    int num_draws = 1;
    long gibbs_previous = draw_gibbs_success + draw_gibbs_discards;
    if (gibbs_previous < draw_gibbs_burnin)
        num_draws += draw_gibbs_burnin - gibbs_previous;
    else if (draw_gibbs_thinning > 1)
        num_draws = draw_gibbs_thinning;

    auto &rng = random::rng();
    boost::random::chi_squared_distribution<double> chisq(n_);
    boost::math::chi_squared_distribution<double> chisq_dist(chisq.n());

    // Pre-calculate and cache the median, as median call is relatively expensive for a chi-squared
    // dist, but having it available lets truncDist avoid an extra cdf call (which is also
    // expensive).
    if (std::isnan(chisq_n_median_) and numRestrictions() > 0) chisq_n_median_ = median(chisq_dist);

    for (int t = 0; t < num_draws; t++) { // num_draws > 1 if thinning or burning in
        // If we discarded the previous value, count it:
        if (t > 0) draw_gibbs_discards++;

        // First take a sigma^2 draw that agrees with the previous z draw
        if (numRestrictions() == 0) {
            // If no restrictions, simple: just draw it from the unconditional distribution
            // NB: gamma(n/2,2) === chisq(v)
            sigma_multiplier = std::sqrt(n_ / chisq(rng));
        }
        else {
            // Otherwise we need to look at the z values we drew in the previous round (or in
            // gibbsInitialize()) and draw an admissable sigma^2 value from the range of values that
            // wouldn't have caused a constraint violation had we used it (instead of the current
            // value) to draw z to form beta = beta() + sigma*Ainv*z.  (See the method documentation for
            // thorough details)

            auto range = sigmaMultiplierRange(z);
            // range now has the minimum and maximum values of sigma (possibly infinite) such that
            // beta() + sigma*Ainv*z doesn't violate the constraints.  What we actually need to
            // generate, however, is beta() + sqrt(u/n)*s*Ainv*z, where u is a chi^2(n) draw (and so
            // the sigma value equals s/sqrt(u/n)).  Since everything else is constant, this in turn
            // puts constraints on u:
            //
            // min <= 1/sqrt(u/n) <= max
            // min^2 <= n / u <= max^2          (squaring)
            // n / max^2 <= u <= n / min^2      (inverting, flipping inequalities, and mult by n)
            double lower_bound = n_ / (range.second * range.second),
                   upper_bound = n_ / (range.first  * range.first);

            // If we get a draw failure here, redrawing won't help (because the draw failure is
            // caused by the betas), but we can try to restart at the previous beta draw, which
            // might help.

            if (range.first > range.second or range.second < 0)
                throw draw_failure("sigma draw failure: no admissable sigma values");
            try {
                sigma_multiplier = std::sqrt(n_ / random::truncDist(chisq_dist, chisq, lower_bound, upper_bound, chisq_n_median_, 0.05, 10));
            }
            catch (const std::runtime_error &df) {
                throw draw_failure(std::string("sigma draw failure: ") + df.what());
            }
        }

        VectorXd newz(z);
        for (unsigned int j = 0; j < K_; j++) {
            // Temporarily set the coefficient to 0, so that the calculation below doesn't
            // include it and we can figure out (from the result) the admissable value limits.
            newz[j] = 0.0;

            // Figure out lj and uj, the most binding constraints on z_j: start from infinity
            // (unbounded)
            double lj = -INFINITY, uj = INFINITY;

            // Calculate all the (translated, z-space) restriction coefficients affecting
            // element newz[j].  Note that we do this inside the loop for just one column,
            // because the value we draw here will affect the next value, so we can't
            // pre-calculating the entire matrix (it changes in each iteration).
            VectorXd Dj = sigma_multiplier * netRestrictionMatrix().col(j);
            // These are the constraint slackness values with newz[j] == 0; we divide by the
            // associated Dj value to translate back from beta-space constraints into z-space constraints.
            VectorXd constraints = (rMinusRBeta() - sigma_multiplier * (netRestrictionMatrix() * newz)).cwiseQuotient(Dj);

            for (size_t r = 0; r < numRestrictions(); r++) {
                if (Dj[r] > 0) { // <= constraint (we didn't flip the direction by dividing because Djr is positive)
                    if (constraints[r] < uj) uj = constraints[r];
                }
                else if (Dj[r] < 0) { // >= constraint (we flipped the constraint direction because Djr is negative)
                    if (constraints[r] > lj) lj = constraints[r];
                }
                // else Dj is 0, which means no constraint applies to j
            }

            // Now lj is the most-binding bottom constraint, uj is the most-binding upper
            // constraint.  Make sure they aren't conflicting:
            if (lj >= uj) throw draw_failure("drawGibbs(): found impossible-to-satisfy linear constraints", *this);

            // Our new Z is a truncated standard normal (truncated by the bounds we just found)
            newz[j] = random::truncated_normal_distribution<double>(0, 1, lj, uj)(rng);
        }
        // newz contains all new draws, replace z with it:
        z = newz;

        // If we get here, we succeeded in the draw, hurray!
        gibbs_last_z_.reset(new VectorXd(z));
    }
    draw_gibbs_success++;

    if (last_draw_.size() != K_ + 1) last_draw_.resize(K_ + 1);

    last_draw_.head(K_) = beta() + toBetaVector(z, sigma_multiplier);
    last_draw_[K_] = sigma_multiplier*sigma_multiplier * s2_;

    return last_draw_;
}

std::pair<double, double> BayesianLinearRestricted::sigmaMultiplierRange(const Eigen::VectorXd &z) const {
    std::pair<double, double> range(0, INFINITY);
    // The current beta = beta() + A^{-1}z, and so is modifying beta() by A^{-1}z:
    VectorXd denom = R() * toBetaVector(z);
    for (size_t i = 0; i < numRestrictions(); i++) {
        if (denom[i] == 0) {
            // This means our z draw was exactly parallel to the restriction line--in which
            // case, any amount of scaling will not violate the restriction (since we
            // already know Z satisfies this restriction), so we don't need to do anything.
            // (This seems extremely unlikely, but just in case).
            continue;
        }
        double limit = rMinusRBeta()[i] / denom[i];

        if (denom[i] > 0) { if (limit < range.second) range.second = limit; }
        else { if (limit > range.first) range.first = limit; }
    }

    return range;
}

const VectorXd& BayesianLinearRestricted::drawRejection(long max_discards) {
    last_draw_mode = DrawMode::Rejection;
    if (max_discards < 0) max_discards = draw_rejection_max_discards;
    draw_rejection_discards_last = 0;
    for (bool redraw = true; redraw; ) {
        redraw = false;
        auto &theta = BayesianLinear::draw();
        if (numRestrictions() > 0) {
            VectorXd Rbeta = restrict_select_ * theta.head(K_);
            if ((Rbeta.array() > restrict_values_.array()).any()) {
                // Restrictions violated
                redraw = true;
                ++draw_rejection_discards_last;
                ++draw_rejection_discards;
                if (draw_rejection_discards_last > max_discards) {
                    throw draw_failure("draw() failed: maximum number of inadmissible draws reached.");
                }
            }
        }
    }

    ++draw_rejection_success;

    return last_draw_;
}

bool BayesianLinearRestricted::hasRestriction(size_t k, bool upper) const {
    for (size_t row = 0; row < numRestrictions(); row++) {
        const double &coef = restrict_select_(row, k);
        // Only look at rows with a non-zero coefficient on k:
        if (coef == 0 or (upper ? coef < 0 : coef > 0)) continue;
        // This row has a restriction of the proper sign on k: check now to make sure it doesn't
        // have any other restrictions (i.e. all other restrictions should be 0)
        if ((restrict_select_.row(row).array() != 0).count() != 1)
            continue;
        return true;
    }
    return false;
}

double BayesianLinearRestricted::getRestriction(size_t k, bool upper) const {
    double most_binding = std::numeric_limits<double>::quiet_NaN();
    for (size_t row = 0; row < numRestrictions(); row++) {
        const double &coef = restrict_select_(row, k);
        // Only look at rows with a non-zero coefficient on k of the right sign
        if (coef == 0 or (upper ? coef < 0 : coef > 0)) continue;
        // This row has a restriction of the proper sign on k: check now to make sure it doesn't
        // have any other restrictions (i.e. all other restrictions should be 0)
        if ((restrict_select_.row(row).array() != 0).count() != 1)
            continue;

        double r = restrict_values_[row] / coef;
        if (std::isnan(most_binding) or (upper ? r < most_binding : r > most_binding))
            most_binding = r;
    }
    return most_binding;
}

BayesianLinearRestricted::RestrictionProxy::RestrictionProxy(BayesianLinearRestricted &lr, size_t k, bool upper)
    : lr_(lr), k_(k), upper_(upper)
{}

bool BayesianLinearRestricted::RestrictionProxy::restricted() const {
    return lr_.hasRestriction(k_, upper_);
}
BayesianLinearRestricted::RestrictionProxy::operator double() const {
    return lr_.getRestriction(k_, upper_);
}

BayesianLinearRestricted::RestrictionProxy& BayesianLinearRestricted::RestrictionProxy::operator=(double r) {
    double Rk = upper_ ? 1.0 : -1.0;
    if (not upper_) r = -r;

    RowVectorXd R = RowVectorXd::Zero(lr_.K());
    R[k_] = Rk;

    lr_.addRestriction(R, r);

    return *this;
}

BayesianLinearRestricted::RestrictionIneqProxy::RestrictionIneqProxy(BayesianLinearRestricted &lr, size_t k)
    : lr_(lr), k_(k)
{}

bool BayesianLinearRestricted::RestrictionIneqProxy::hasUpperBound() const {
    return lr_.hasRestriction(k_, true);
}

double BayesianLinearRestricted::RestrictionIneqProxy::upperBound() const {
    return lr_.getRestriction(k_, true);
}

bool BayesianLinearRestricted::RestrictionIneqProxy::hasLowerBound() const {
    return lr_.hasRestriction(k_, false);
}

double BayesianLinearRestricted::RestrictionIneqProxy::lowerBound() const {
    return lr_.getRestriction(k_, false);
}

BayesianLinearRestricted::RestrictionIneqProxy& BayesianLinearRestricted::RestrictionIneqProxy::operator<=(double r) {
    RowVectorXd R = RowVectorXd::Zero(lr_.K());
    R[k_] = 1.0;
    lr_.addRestriction(R, r);
    return *this;
}

BayesianLinearRestricted::RestrictionIneqProxy& BayesianLinearRestricted::RestrictionIneqProxy::operator>=(double r) {
    RowVectorXd R = RowVectorXd::Zero(lr_.K());
    R[k_] = -1.0;
    lr_.addRestriction(R, -r);
    return *this;
}

BayesianLinearRestricted::operator std::string() const {
    std::ostringstream os;
    os << BayesianLinear::operator std::string();
    if (numRestrictions() == 0) os << "  No restrictions.\n";
    else if (numRestrictions() == 1) os << "  1 restriction:\n";
    else os << "  " << numRestrictions() << " restrictions:\n";
    for (unsigned r = 0; r < numRestrictions(); r++) {
        bool first = true;
        // First go through the row and find out if every element is negative (or zero): if so,
        // we'll negate all the values and report it as a >= restriction.
        bool negate = false;
        for (unsigned t = 0; t < K_; t++) {
            const double &z = restrict_select_(r, t);
            if (z < 0) { negate = true; }
            else if (z > 0) { negate = false; break; }
        }

        for (unsigned int j = 0; j < K_; j++) {
            double d = restrict_select_(r, j);
            if (d != 0) {
                if (negate) d = -d;
                if (first) {
                    first = false;
                    os << "    ";
                    if (d < 0) { os << "-"; d = -d; }
                }
                else {
                    if (d < 0) { os << " - "; d = -d; }
                    else       { os << " + "; }
                }
                if (d != 1) os << d << " ";
                os << "beta[" << j << "]";
            }
        }
        // If there were no non-zero elements then this is a trivial restriction of 0 <= r.
        if (first) { os << "    0"; negate = false; }

        os << (negate ? u8" ⩾ " : u8" ⩽ ") << (negate ? -1 : 1) * restrict_values_[r] << "\n";
    }

    return os.str();
}

std::string BayesianLinearRestricted::display_name() const { return "BayesianLinearRestricted"; }

}}
