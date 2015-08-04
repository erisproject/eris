#include <eris/belief/BayesianLinearRestricted.hpp>
#include <cmath>
#include <Eigen/QR>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/chi_squared.hpp>

#include <eris/debug.hpp>

using namespace Eigen;

namespace eris { namespace belief {

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

void BayesianLinearRestricted::allocateRestrictions(size_t more) {
    // Increment by K_ rows at a time.  (This is fairly arbitrary, but at least the number of
    // restrictions will typically be correlated with the number of regressors)
    size_t rows = K_ * (size_t) std::ceil((restrict_size_ + more) / (double) K_);
    if (restrict_select_.cols() != K_) restrict_select_.conservativeResize(rows, K_);
    else if ((size_t) restrict_select_.rows() < restrict_size_ + more) {
        restrict_select_.conservativeResize(rows, NoChange);
    }
    if ((size_t) restrict_values_.size() < restrict_size_ + more) {
        restrict_values_.conservativeResize(rows);
    }
}

void BayesianLinearRestricted::addRestriction(const Ref<const RowVectorXd> &R, double r) {
    if (R.size() != K_) throw std::logic_error("Unable to add linear restriction: R does not have size K");
    allocateRestrictions(1);
    restrict_values_[restrict_size_] = r;
    restrict_select_.row(restrict_size_) = R;
    restrict_size_++;
    reset();
}

void BayesianLinearRestricted::addRestrictionGE(const Ref<const RowVectorXd> &R, double r) {
    return addRestriction(-R, -r);
}

void BayesianLinearRestricted::addRestrictions(const Ref<const MatrixXd> &R, const Ref<const VectorXd> &r) {
    if (R.cols() != K_) throw std::logic_error("Unable to add linear restrictions: R does not have K columns");
    auto num_restr = R.rows();
    if (num_restr != r.size()) throw std::logic_error("Unable to add linear restrictions: different number of rows in R and r");
    allocateRestrictions(num_restr);
    restrict_values_.segment(restrict_size_, num_restr) = r;
    restrict_select_.middleRows(restrict_size_, num_restr) = R;
    restrict_size_ += num_restr;
    reset();
}

void BayesianLinearRestricted::addRestrictionsGE(const Ref<const MatrixXd> &R, const Ref<const VectorXd> &r) {
    return addRestrictions(-R, -r);
}

void BayesianLinearRestricted::clearRestrictions() {
    restrict_size_ = 0;
    reset();
    // Don't need to resize restrict_*, the values aren't used if restrict_size_ = 0
}

void BayesianLinearRestricted::removeRestriction(size_t r) {
    if (r >= restrict_size_) throw std::logic_error("BayesianLinearRestricted::removeRestriction(): invalid restriction row `" + std::to_string(r) + "' given");
    for (size_t row = r; row < restrict_size_; row++) {
        restrict_select_.row(row) = restrict_select_.row(row+1);
        restrict_values_[row] = restrict_values_[row+1];
    }
    --restrict_size_;
}

void BayesianLinearRestricted::reset() {
    BayesianLinear::reset();
    draw_rejection_discards_last = 0;
    draw_rejection_discards = 0;
    draw_rejection_success = 0;
    gibbs_D_.reset();
    gibbs_last_z_.reset();
    gibbs_r_Rbeta_.reset();
    gibbs_last_sigma_ = std::numeric_limits<double>::signaling_NaN();
    gibbs_draws_ = 0;
    chisq_n_median_ = std::numeric_limits<double>::signaling_NaN();
}

Block<const MatrixXdR, Dynamic, Dynamic, true> BayesianLinearRestricted::R() const {
    return restrict_select_.topRows(restrict_size_);
}

VectorBlock<const VectorXd> BayesianLinearRestricted::r() const {
    return restrict_values_.head(restrict_size_);
}

const VectorXd& BayesianLinearRestricted::draw() {
    return draw(draw_mode);
}

const VectorXd& BayesianLinearRestricted::draw(DrawMode m) {
    // If they explicitly want rejection draw, do it:
    if (m == DrawMode::Rejection)
        return drawRejection();

    // In auto mode we might try rejection sampling first
    if (m == DrawMode::Auto) { // Need success rate < 0.1 to switch, with at least 20 samples.
        long draw_rej_samples = draw_rejection_success + draw_rejection_discards;
        double success_rate = draw_rejection_success / (double)draw_rej_samples;
        if (success_rate < draw_auto_min_success_rate and draw_rej_samples >= draw_rejection_max_discards) {
            // Too many failures, switch to Gibbs sampling
            m = DrawMode::Gibbs;
        }
        else {
            // Figure out how many sequential failed draws we'd need to hit the failure threshold,
            // then try up to that many times.
            long draw_tries = std::max<long>(
                    std::ceil(draw_rejection_success / draw_auto_min_success_rate),
                    draw_rejection_max_discards)
                - draw_rej_samples;
            try {
                return drawRejection(draw_tries);
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

void BayesianLinearRestricted::gibbsInitialize(const Ref<const VectorXd> &initial, unsigned long max_tries) {
    constexpr double overshoot = 1.5;

    if (initial.size() < K_ or initial.size() > K_+1) throw std::logic_error("BayesianLinearRestricted::gibbsInitialize() called with invalid initial vector (initial.size() != K())");

    gibbs_draws_ = 0;

    const MatrixXd &A = VinvCholL();
    auto &rng = Random::rng();

    if (restrict_size_ == 0) {
        // No restrictions, nothing special to do!
        if (not gibbs_last_z_ or gibbs_last_z_->size() != K_) gibbs_last_z_.reset(new VectorXd(K_));
        *gibbs_last_z_ = A / std::sqrt(s2_) * (initial.head(K_) - beta_);
    }
    else {
        // Otherwise we'll start at the initial value and update
        VectorXd adjusted(initial.head(K_));
        std::vector<size_t> violations;
        violations.reserve(restrict_size_);

        for (unsigned long trial = 1; ; trial++) {
            violations.clear();
            VectorXd v = R() * adjusted - r();
            for (size_t i = 0; i < restrict_size_; i++) {
                if (v[i] > 0) violations.push_back(i);
            }
            if (violations.size() == 0) break; // Restrictions satisfied!
            else if (trial >= max_tries) { // Too many tries: give up
                // Clear gibbs_last_ on failure (so that the next drawGibbs() won't try to use it)
                gibbs_last_z_.reset();
                throw constraint_failure("gibbsInitialize() couldn't find a way to satisfy the model constraints");
            }

            // Select a random constraint to fix:
            size_t fix = violations[std::uniform_int_distribution<size_t>(0, violations.size()-1)(rng)];

            // Aim at the nearest point on the boundary and overshoot (by 50%):
            adjusted += -overshoot * v[fix] / restrict_select_.row(fix).squaredNorm() * restrict_select_.row(fix).transpose();
        }

        if (not gibbs_last_z_ or gibbs_last_z_->size() != K_) gibbs_last_z_.reset(new VectorXd(K_));
        *gibbs_last_z_ = A / std::sqrt(s2_) * (adjusted - beta_);
    }

    // Don't set the last sigma draw; drawGibbs will do that.
    gibbs_last_sigma_ = std::numeric_limits<double>::signaling_NaN();
}

const VectorXd& BayesianLinearRestricted::drawGibbs() {
    last_draw_mode = DrawMode::Gibbs;

    const MatrixXd &Ainv = VcholL();
    double s = std::sqrt(s2_);

    if (not gibbs_D_) gibbs_D_.reset(
            restrict_size_ == 0
            ? new MatrixXdR(0, K_) // It's rather pointless to use drawGibbs() with no restrictions, but allow it for debugging purposes
            : new MatrixXdR(s * R() * Ainv));
    const auto &D = *gibbs_D_;

    if (not gibbs_r_Rbeta_) gibbs_r_Rbeta_.reset(new VectorXd(r() - R() * beta_));
    const auto &r_minus_Rbeta_ = *gibbs_r_Rbeta_;

    if (not gibbs_last_z_) {
        // If we don't have an initial value, draw an *untruncated* value and give it to
        // gibbsInitialize() to fix up.
        for (int trial = 1; ; trial++) {
            try { gibbsInitialize(BayesianLinear::draw(), 10*restrict_size_); break; }
            catch (constraint_failure&) {
                if (trial >= 10) throw;
                // Otherwise try again
            }
        }

        // gibbs_last_*_ will be set up now (or else we threw an exception)
    }
    // Start with z from the last z draw
    VectorXd z(*gibbs_last_z_);
    double sigma = 0;
    double s_sigma = 0;
    auto &rng = Random::rng();

    int num_draws = 1;
    if (gibbs_draws_ < draw_gibbs_burnin)
        num_draws += draw_gibbs_burnin - gibbs_draws_;
    else if (draw_gibbs_thinning > 1)
        num_draws = draw_gibbs_thinning;

    std::chi_squared_distribution<double> chisq(n_);
    boost::math::normal_distribution<double> stdnorm_dist(0, 1);
    boost::math::chi_squared_distribution<double> chisq_dist(chisq.n());
    // Calculate the median just once, as the median call is slightly expensive for a chi-squared dist,
    // but having the median avoids potential extra cdf calls below.
    if (std::isnan(chisq_n_median_) and restrict_size_ > 0) chisq_n_median_ = median(chisq_dist);

    int draw_failures = 0;

    for (int t = 0; t < num_draws; t++) { // num_draws > 1 if thinning or burning in

        // First take a sigma^2 draw that agrees with the previous z draw
        if (restrict_size_ == 0) {
            // If no restrictions, simple: just draw it from the unconditional distribution
            // NB: gamma(n/2,2) === chisq(v)
            sigma = std::sqrt(n_ / chisq(rng));
        }
        else {
            // Otherwise we need to look at the z values we drew in the previous round (or in
            // gibbsInitialize() and draw an admissable sigma^2 value from the range of values that
            // wouldn't have caused a constraint violation had we used it to form beta = beta_ +
            // sigma*s*Ainv*z.  (See the method documentation for thorough details)

            auto range = sigmaRange(z);

            // If we get a draw failure here, redrawing won't help (because the draw failure is
            // caused by the betas), but we can try to restart at the previous beta draw, which
            // might help.
            try {
                sigma = std::sqrt(n_ / Random::truncDist(chisq_dist, chisq, n_ / (range.second*range.second), n_ / (range.first*range.first), chisq_n_median_, 0.05, 10));
            }
            catch (const std::runtime_error &df) {
                if (gibbs_2nd_last_z_) {
                    range = sigmaRange(*gibbs_2nd_last_z_);

                    // If it fails again, there's nothing we can do, so just wrap it up in a
                    // draw_failure and rethrow it.
                    try {
                        sigma = std::sqrt(n_ / Random::truncDist(chisq_dist, chisq, n_ / (range.second*range.second), n_ / (range.first*range.first), chisq_n_median_, 0.05, 10));
                    }
                    catch (const std::runtime_error &df) {
                        throw draw_failure(df.what());
                    }
                }
                else {
                    throw draw_failure(df.what()); // Don't have a previous beta to save us, so just throw with the underlying exception message
                }
            }

            // We want sigma s.t. beta_ + sigma * (s * A) * z has the right distribution for a
            // multivariate t, which is sigma ~ sqrt(n_ / chisq(n_)), *but* truncated to [sigma_l,
            // sigma_u].  To accomplish that truncation for sigma, we need to truncate the chisq(n_)
            // to [n_/sigma_u^2, n/sigma_l^2].

        }

        s_sigma = sigma*s;

        try {
            for (unsigned int j = 0; j < K_; j++) {
                // Temporarily set the coefficient to 0, so that we don't have to maintain a bunch of
                // one-column-removed vectors and matrices below
                z[j] = 0.0;

                // Figure out l_j and u_j, the most binding constraints on z_j
                double lj = -INFINITY, uj = INFINITY;
                for (size_t r = 0; r < restrict_size_; r++) {
                    // NB: not calculating the whole LHS vector and RHS vector at once, because there's
                    // a good chance of 0's in the LHS vector, in which case we don't need to bother
                    // calculting the RHS at all
                    auto &dj = D(r, j);
                    if (dj != 0) {
                        // Take the other z's as given, find the range for this one
                        double constraint = (r_minus_Rbeta_[r] / sigma - (D.row(r) * z)) / dj;
                        if (dj > 0) { // <= constraint (we didn't flip the sign by dividing by dj)
                            if (constraint < uj) uj = constraint;
                        }
                        else { // >= constraint
                            if (constraint > lj) lj = constraint;
                        }
                    }
                }

                // Now lj is the most-binding bottom constraint, uj is the most-binding upper
                // constraint.  Make sure they aren't conflicting:
                if (lj >= uj) throw draw_failure("drawGibbs(): found impossible-to-satisfy linear constraints", *this);

                // Our new Z is a truncated standard normal (truncated by the bounds we just found)
                try {
                    z[j] = Random::truncDist(stdnorm_dist, Random::stdnorm, lj, uj, 0.0);
                }
                catch (const std::runtime_error &fail) {
                    // If the truncated normal fails, wrap in a draw failure and rethrow:
                    throw draw_failure("drawGibbs(): unable to draw appropriate truncated normal: " + std::string(fail.what()));
                }
            }
        }
        catch (const draw_failure &df) {
            draw_failures++;
            // Allow up to `draw_gibbs_retry` consecutive failures
            if (draw_failures > draw_gibbs_retry) throw;
            // Retry:
            t--;
            continue;
        }

        // If we get here, we succeeded in the draw, hurray!
        gibbs_2nd_last_z_ = gibbs_last_z_;
        gibbs_last_z_.reset(new VectorXd(z));
        gibbs_last_sigma_ = sigma;
        gibbs_draws_++;
        draw_failures = 0;
    }

    if (last_draw_.size() != K_ + 1) last_draw_.resize(K_ + 1);

    last_draw_.head(K_) = beta_ + s_sigma * Ainv * z;
    last_draw_[K_] = s_sigma * s_sigma;

    return last_draw_;
}

std::pair<double, double> BayesianLinearRestricted::sigmaRange(const Eigen::VectorXd &z) {
    std::pair<double, double> range(0, INFINITY);
    for (size_t i = 0; i < restrict_size_; i++) {
        double denom = gibbs_D_->row(i) * z;
        if (denom == 0) {
            // This means our z draw was exactly parallel to the restriction line--in which
            // case, any amount of scaling will not violate the restriction (since we
            // already know Z satisfies this restriction), so we don't need to do anything.
            // (This case seems extremely unlikely, but just in case).
            continue;
        }
        double limit = (*gibbs_r_Rbeta_)[i] / denom;

        if (denom > 0) { if (limit < range.second) range.second = limit; }
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
        if (restrict_size_ > 0) {
            VectorXd Rbeta = restrict_select_.topRows(restrict_size_) * theta.head(K_);
            if ((Rbeta.array() > restrict_values_.head(restrict_size_).array()).any()) {
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
    for (size_t row = 0; row < restrict_size_; row++) {
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
    for (size_t row = 0; row < restrict_size_; row++) {
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
    if (restrict_size_ == 0) os << "  No restrictions.\n";
    else if (restrict_size_ == 1) os << "  1 restriction:\n";
    else os << "  " << restrict_size_ << " restrictions:\n";
    for (size_t r = 0; r < restrict_size_; r++) {
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
