#include <eris/belief/BayesianLinear.hpp>
#include <eris/Random.hpp>
#include <Eigen/QR>
#include <Eigen/SVD>
#include <Eigen/Cholesky>
#include <cmath>
#include <algorithm>

namespace eris { namespace belief {

using namespace Eigen;

constexpr double BayesianLinear::NONINFORMATIVE_N, BayesianLinear::NONINFORMATIVE_S2, BayesianLinear::NONINFORMATIVE_Vc;

BayesianLinear::BayesianLinear(
        const Ref<const VectorXd> beta,
        double s2,
        const Ref<const MatrixXd> V_inverse,
        double n)
    : beta_(beta), s2_{s2}, V_inv_(V_inverse.selfadjointView<Lower>()), n_{n}, K_(beta_.rows())
{
    // Check that the given matrices conform
    checkLogic();
}

BayesianLinear::BayesianLinear(unsigned int K, const Ref<const MatrixXdR> noninf_X, const Ref<const VectorXd> noninf_y) :
    beta_{VectorXd::Zero(K)}, s2_{NONINFORMATIVE_S2}, V_inv_{1.0/NONINFORMATIVE_Vc * MatrixXd::Identity(K, K)},
    n_{NONINFORMATIVE_N}, noninformative_{true}, K_{K}
{
    if (K < 1) throw std::logic_error("BayesianLinear model requires at least one parameter");
    auto fixed = fixedModelSize();
    if (fixed and K != fixed) throw std::logic_error("BayesianLinear model constructed with incorrect number of model parameters");

    if (noninf_X.rows() != noninf_y.rows()) throw std::logic_error("Partially informed model construction error: X.rows() != y.rows()");
    if (noninf_X.rows() > 0) {
        if (noninf_X.cols() != K_) throw std::logic_error("Partially informed model construction error: X.cols() != K");
        noninf_X_.reset(new MatrixXdR(noninf_X));
        noninf_y_.reset(new VectorXd(noninf_y));
    }
}


void BayesianLinear::checkLogic() {
    auto k = K();
    if (k < 1) throw std::logic_error("BayesianLinear model requires at least one parameter");
    if (V_inv_.rows() != V_inv_.cols()) throw std::logic_error("BayesianLinear requires square V_inverse matrix");
    if (k != V_inv_.rows()) throw std::logic_error("BayesianLinear requires beta and V_inverse of same number of rows");
    if (V_inv_chol_L_ and (V_inv_chol_L_->rows() != V_inv_chol_L_->cols() or V_inv_chol_L_->rows() != k))
        throw std::logic_error("BayesianLinear constructed with invalid VinvCholL() matrix");
    auto fixed = fixedModelSize();
    if (fixed and k != fixed) throw std::logic_error("BayesianLinear model constructed with incorrect number of model parameters");
}

unsigned int BayesianLinear::fixedModelSize() const { return 0; }

#define NO_EMPTY_MODEL if (K_ == 0) { throw std::logic_error("Cannot use default constructed model object as a model"); }

const VectorXd& BayesianLinear::beta() const { NO_EMPTY_MODEL; return beta_; }
const double& BayesianLinear::s2() const { NO_EMPTY_MODEL; return s2_; }
const double& BayesianLinear::n() const { NO_EMPTY_MODEL; return n_; }
const MatrixXd& BayesianLinear::Vinv() const { NO_EMPTY_MODEL; return V_inv_; }
const MatrixXd& BayesianLinear::VcholL() const {
    NO_EMPTY_MODEL;
    if (not V_chol_L_)
        V_chol_L_ = std::make_shared<MatrixXd>(V_inv_.fullPivHouseholderQr().inverse().llt().matrixL());
    return *V_chol_L_;
}
const MatrixXd& BayesianLinear::VinvCholL() const {
    NO_EMPTY_MODEL;
    if (not V_inv_chol_L_)
        V_inv_chol_L_ = std::make_shared<MatrixXd>(V_inv_.selfadjointView<Lower>().llt().matrixL());
    return *V_inv_chol_L_;
}
const FullPivHouseholderQR<MatrixXd>& BayesianLinear::VinvCholLqr() const {
    NO_EMPTY_MODEL;
    if (not V_inv_chol_L_qr_)
        V_inv_chol_L_qr_ = std::make_shared<FullPivHouseholderQR<MatrixXd>>(VinvCholL());
    return *V_inv_chol_L_qr_;
}

const bool& BayesianLinear::noninformative() const { NO_EMPTY_MODEL; return noninformative_; }

const std::vector<std::string>& BayesianLinear::names() const {
    if (not beta_names_ or beta_names_->size() != K()) {
        beta_names_.reset(new std::vector<std::string>);
        for (unsigned int i = 0; i < K_; i++) {
            beta_names_->push_back(std::to_string(i));
        }
    }
    return *beta_names_;
}

void BayesianLinear::names(const std::vector<std::string> &names) {
    if (names.empty()) {
        beta_names_.reset();
        beta_names_default_ = true;
        return;
    }
    else if (names.size() != K_) throw std::domain_error("BayesianLinear::names(): given names vector is not of size K");

    beta_names_.reset(new std::vector<std::string>());
    beta_names_->reserve(K_);
    for (const auto &n : names) beta_names_->push_back(n);
    beta_names_default_ = false;
}

VectorXd BayesianLinear::predict(const Ref<const MatrixXd> &X, unsigned int draws) {
    VectorXd y;
    y = predictGeneric(X, [](double y) { return y; }, draws);
    return y;
}

MatrixXd BayesianLinear::predictVariance(const Ref<const MatrixXd> &X, unsigned int draws) {
    if (draws == 1 or (draws == 0 and prediction_draws_.cols() == 1))
        throw std::logic_error("predictVariance() cannot calculate variance using only 1 draw");

    std::vector<std::function<double(double)>> g;
    g.emplace_back([](double y) { return y; }); // Mean
    g.emplace_back([](double y) { return y*y; }); // E[y^2]

    MatrixXd results = predictGeneric(X, g, draws);
    results.col(1) =
        draws / (double)(draws-1) * // Correct for sample variance bias
        (results.col(1) - results.col(0).array().square().matrix()); // E[y^2] - E[y]^2
    return results;
}

MatrixXd BayesianLinear::predictGeneric(const Ref<const MatrixXd> &X, const std::vector<std::function<double(double y)>> &g, unsigned draws) {
    if (noninformative_)
        throw std::logic_error("Cannot call predict using a noninformative model");
    if (g.empty())
        throw std::logic_error("predictGeneric() called without any g() functions");

    if (draws == 0) draws = prediction_draws_.cols() > 0 ? prediction_draws_.cols() : 1000;

    if (draws > prediction_draws_.cols()) {
        unsigned i = prediction_draws_.cols();
        prediction_draws_.conservativeResize(K_+1, draws);
        for (; i < draws; i++) {
            prediction_draws_.col(i) = draw();
        }
    }

    // Draw new error terms, if needed.
    unsigned err_cols = prediction_errors_.cols(), err_rows = prediction_errors_.rows();
    // Need more rows:
    if (err_rows < X.rows()) {
        err_rows = X.rows();
        // We need new rows, but have to make sure that errors doesn't have more columns than
        // draws (because we can't draw new errors for those columns)
        if (err_cols > prediction_draws_.cols()) err_cols = prediction_draws_.cols();
    }

    // Need more columns:
    if (err_cols < prediction_draws_.cols()) {
        err_cols = prediction_draws_.cols();
    }

    auto &rng = Random::rng();
    if (err_cols != prediction_errors_.cols() or err_rows != prediction_errors_.rows()) {
        // Keep track of where we need to start filling:
        unsigned startc = prediction_errors_.cols();
        unsigned startr = prediction_errors_.rows();
        prediction_errors_.conservativeResize(err_rows, err_cols);
        for (unsigned c = (startr < err_rows ? 0 : startc); c < err_cols; c++) {
            // For the first startc columns, we need to draw values for any new rows; for startc and
            // beyond we need to draw values for every row:
            std::normal_distribution<double> err(0, std::sqrt(prediction_draws_(K_, c)));
            for (unsigned r = (c < startc ? startr : 0); r < err_rows; r++) {
                prediction_errors_(r, c) = err(rng);
            }
        }
    }

    MatrixXd results = MatrixXd::Zero(X.rows(), g.size());
    VectorXd ydraw;
    for (unsigned i = 0; i < draws; i++) {
        ydraw = X * prediction_draws_.col(i).head(K_) + prediction_errors_.col(i).head(X.rows());
        for (unsigned t = 0; t < ydraw.size(); t++) for (unsigned gi = 0; gi < g.size(); gi++) {
            results(t, gi) += g[gi](ydraw[t]);
        }
    }

    return results / draws;
}

MatrixXd BayesianLinear::predictGeneric(const Ref<const MatrixXd> &X, const std::function<double(double)> &g, unsigned draws) {
    std::vector<std::function<double(double)>> gvect;
    gvect.push_back(g);
    return predictGeneric(X, gvect, draws);
}

MatrixXd BayesianLinear::predictGeneric(const Ref<const MatrixXd> &X, const std::function<double(double)> &g0, const std::function<double(double)> &g1, unsigned draws) {
    std::vector<std::function<double(double)>> gvect;
    gvect.push_back(g0);
    gvect.push_back(g1);
    return predictGeneric(X, gvect, draws);
}

const VectorXd& BayesianLinear::draw() {
    NO_EMPTY_MODEL;

    if (last_draw_.size() != K_ + 1) last_draw_.resize(K_ + 1);

    auto &rng = Random::rng();

    // (beta,h) is distributed as a normal-gamma(beta, V, s2^{-1}, n), in Koop's Gamma distribution
    // notation, or NG(beta, V, n/2, 2*s2^{-1}/n) in the more common G(shape,scale) notation
    // (which std::gamma_distribution wants).
    //
    // Proof:
    // Let $G_{k\theta}(k,\theta)$ be the shape ($k$), scale ($\theta$) notation.  This has mean $k\theta$ and
    // variance $k\theta^2$.
    //
    // Let $G_{Koop}(\mu,\nu)$ be Koop's notation, where $\mu$ is the mean and $\nu$ is the degrees of
    // freedom, which has variance $\frac{2\mu^2}{\nu}$.  Equating means and variances:
    //
    // \[
    //     k\theta = \mu
    //     k\theta^2 = \frac{2\mu^2}{\nu}
    //     \theta = \frac{2\mu}{\nu}
    //     k = \frac{2}{\nu}
    // \]
    // where the third equation follows from the first divided by the second, and fourth follows
    // from the first divided by the third.  Thus
    // \[
    //     G_{Koop}(\mu,\nu) = G_{k\theta}(\frac{2}{\nu},\frac{2\mu}{\nu})
    // \]

    // To draw this, first draw a gamma-distributed "h" value (store its inverse)
    last_draw_[K_] = n_ * s2_ / std::chi_squared_distribution<double>(n_)(rng);

    // Now use that to draw a multivariate normal conditional on h, with mean beta and variance
    // h^{-1} V; this is the beta portion of the draw:
    last_draw_.head(K_) = multivariateNormal(beta_, VcholL(), std::sqrt(last_draw_[K_]));

    return last_draw_;
}

VectorXd BayesianLinear::multivariateNormal(const Ref<const VectorXd> &mu, const Ref<const MatrixXd> &L, double s) {
    if (mu.rows() != L.rows() or L.rows() != L.cols())
        throw std::logic_error("multivariateNormal() called with non-conforming mu and L");

    // To draw such a normal, we need the lower-triangle Cholesky decomposition L of V, and a vector
    // of K random \f$N(\mu=0, \sigma^2=h^{-1})\f$ values.  Then \f$beta + Lz\f$ yields a \f$beta\f$
    // draw of the desired distribution.
    VectorXd z(mu.size());
    for (unsigned int i = 0; i < z.size(); i++) z[i] = Random::rstdnorm();

    return mu + L * (s * z);
}

VectorXd BayesianLinear::multivariateT(const Ref<const VectorXd> &mu, double nu, const Ref<const MatrixXd> &L, double s) {
    VectorXd y(multivariateNormal(VectorXd::Zero(mu.size()), L, s));

    double u = std::chi_squared_distribution<double>(nu)(Random::rng());

    return mu + std::sqrt(nu / u) * y;
}

const VectorXd& BayesianLinear::lastDraw() const {
    return last_draw_;
}

void BayesianLinear::discard() {
    NO_EMPTY_MODEL;
    if (prediction_draws_.cols() > 0) prediction_draws_.resize(NoChange, 0);
    if (prediction_errors_.cols() > 0) prediction_errors_.resize(0, 0);
}

std::ostream& operator<<(std::ostream &os, const BayesianLinear &b) {
    return os << (std::string) b;
}

BayesianLinear::operator std::string() const {
    std::ostringstream summary;
    summary << display_name();
    if (K_ == 0) summary << " model with no parameters (default constructed)";
    else {
        if (noninformative()) summary << " (noninformative)";
        summary << " model: K=" << K_ << ", n=" << n_ << ", s2=" << s2_;
        if (not beta_names_default_) {
            summary << "\n  X cols:";
            for (auto &n : names()) {
                if (n.find_first_of(" \t\n") != n.npos) summary << " {" << n << "}";
                else summary << " " << n;
            }
        }
        summary <<
            "\n  beta = " << beta_.transpose().format(IOFormat(StreamPrecision, 0, ", ")) <<
            "\n  V = " << V_inv_.fullPivHouseholderQr().inverse().format(IOFormat(6, 0, " ", "\n      ")) << "\n";
    }
    return summary.str();
}

std::string BayesianLinear::display_name() const { return "BayesianLinear"; }

void BayesianLinear::verifyParameters() const { NO_EMPTY_MODEL; }

// Posterior constructor (prior copy version)
BayesianLinear::BayesianLinear(
        const BayesianLinear &prior,
        const Ref<const VectorXd> &y,
        const Ref<const MatrixXd> &X,
        double w) : BayesianLinear(prior) {
    if (w != 1.0) weakenInPlace(w);
    updateInPlace(y, X);
}

// Posterior constructor (prior move version)
BayesianLinear::BayesianLinear(
        BayesianLinear &&prior,
        const Ref<const VectorXd> &y,
        const Ref<const MatrixXd> &X,
        double w) : BayesianLinear(std::move(prior)) {
    if (w != 1.0) weakenInPlace(w);
    updateInPlace(y, X);
}

// Posterior constructor, just weakening (prior copy version)
BayesianLinear::BayesianLinear(const BayesianLinear &prior, double w) : BayesianLinear(prior) {
    // NB: call even if w == 1, unlike above, because this does a needed reset()--above the
    // updateInPlace also does a reset().
    weakenInPlace(w);
}

// Posterior constructor, just weakening (prior move version)
BayesianLinear::BayesianLinear(BayesianLinear &&prior, double w) : BayesianLinear(std::move(prior)) {
    // NB: call even if w == 1, unlike above, because this does a needed reset()--above the
    // updateInPlace also does a reset().
    weakenInPlace(w);
}

void BayesianLinear::updateInPlace(const Ref<const VectorXd> &y, const Ref<const MatrixXd> &X) {
    NO_EMPTY_MODEL;
    if (y.rows() != X.rows())
        throw std::logic_error("update(y, X) failed: y and X are non-conformable");
    if (X.rows() > 0 and X.cols() != K())
        throw std::logic_error("update(y, X) failed: X has wrong number of columns (expected " + std::to_string(K()) + ", got " + std::to_string(X.cols()) + ")");

    reset();

    if (y.rows() == 0) // Nothing to update!
        return;

    if (noninformative_) {
        if (not noninf_X_ or noninf_X_->rows() == 0) noninf_X_.reset(new MatrixXdR(X.rows(), K()));
        else if (not noninf_X_.unique()) {
            auto old_x = noninf_X_;
            noninf_X_.reset(new MatrixXdR(old_x->rows() + X.rows(), K()));
            noninf_X_->topRows(old_x->rows()) = *old_x;
        }
        else noninf_X_->conservativeResize(noninf_X_->rows() + X.rows(), K());

        if (not noninf_X_unweakened_ or noninf_X_unweakened_->rows() == 0) noninf_X_unweakened_.reset(new MatrixXdR(X.rows(), K()));
        else if (not noninf_X_unweakened_.unique()) {
            auto old_x = noninf_X_unweakened_;
            noninf_X_unweakened_.reset(new MatrixXdR(old_x->rows() + X.rows(), K()));
            noninf_X_unweakened_->topRows(old_x->rows()) = *old_x;
        }
        else noninf_X_unweakened_->conservativeResize(noninf_X_unweakened_->rows() + X.rows(), K());


        if (not noninf_y_ or noninf_y_->rows() == 0) noninf_y_.reset(new VectorXd(y.rows()));
        else if (not noninf_y_.unique()) {
            auto old_y = noninf_y_;
            noninf_y_.reset(new VectorXd(old_y->rows() + y.rows()));
            noninf_y_->head(old_y->rows()) = *old_y;
        }
        else noninf_y_->conservativeResize(noninf_y_->rows() + y.rows());

        if (not noninf_y_unweakened_ or noninf_y_unweakened_->rows() == 0) noninf_y_unweakened_.reset(new VectorXd(y.rows()));
        else if (not noninf_y_unweakened_.unique()) {
            auto old_y = noninf_y_unweakened_;
            noninf_y_unweakened_.reset(new VectorXd(old_y->rows() + y.rows()));
            noninf_y_unweakened_->head(old_y->rows()) = *old_y;
        }
        else noninf_y_unweakened_->conservativeResize(noninf_y_unweakened_->rows() + y.rows());

        noninf_X_->bottomRows(X.rows()) = X;
        noninf_X_unweakened_->bottomRows(X.rows()) = X;
        noninf_y_->tail(y.rows()) = y;
        noninf_y_unweakened_->tail(y.rows()) = y;

        if (noninf_X_->rows() > K()) {
            MatrixXd XtX = (noninf_X_->transpose() * *noninf_X_).selfadjointView<Lower>();
            auto qr = XtX.fullPivHouseholderQr();
            if (qr.rank() >= K()) {
                beta_ = noninf_X_->jacobiSvd(ComputeThinU | ComputeThinV).solve(*noninf_y_);
                V_inv_ = std::move(XtX);
                n_ = noninf_X_->rows();
                s2_ = (*noninf_y_unweakened_ - *noninf_X_unweakened_ * beta_).squaredNorm() / n_;

                // These are unlikely to be set since this model was noninformative, but just in case:
                if (V_chol_L_) V_chol_L_.reset();
                if (V_inv_chol_L_) V_inv_chol_L_.reset();
                if (V_inv_chol_L_qr_) V_inv_chol_L_qr_.reset();
                noninf_X_.reset();
                noninf_y_.reset();
                noninf_X_unweakened_.reset();
                noninf_y_unweakened_.reset();
                noninformative_ = false; // We aren't noninformative anymore!
            }
        }
    }
    else {
        // Otherwise we were already informative, so just pass the data along.
        updateInPlaceInformative(y, X);
    }
}

void BayesianLinear::updateInPlaceInformative(const Ref<const VectorXd> &y, const Ref<const MatrixXd> &X) {
    MatrixXd Xt = X.transpose();

    // betanew = Vnew (Vold^{-1}*betaold + X'y), but we don't want to actually do a matrix inverse,
    // so calculate the inside first (this is essentially the X'y term for the entire data)
    VectorXd inside = V_inv_ * beta_ + Xt * y;

    // New V^{-1} is just the old one plus the new data X'X:
    MatrixXd V_inv_post = V_inv_ + Xt * X;

    // Now get new beta:
    VectorXd beta_post(V_inv_post.fullPivHouseholderQr().solve(inside));

    double n_prior = n_;
    n_ += X.rows();

    // Now calculate the residuals for the new data:
    VectorXd residualspost = y - X * beta_post;

    // And get the difference between new and old beta:
    VectorXd beta_diff = beta_post - beta_;

    double s2_prior_beta_delta = beta_diff.transpose() * V_inv_ * beta_diff;
    s2_prior_beta_delta *= pending_weakening_;
    pending_weakening_ = 1.0;

    // Calculate new s2 from SSR of new data plus n times previous s2 (==SSR before) plus the change
    // to the old SSR that would are a result of beta changing:
    s2_ = (residualspost.squaredNorm() + n_prior * s2_ +  s2_prior_beta_delta) / n_;

    beta_ = std::move(beta_post);
    V_inv_ = std::move(V_inv_post);

    // The decompositions will have to be recalculated, if set:
    if (V_chol_L_) V_chol_L_.reset();
    if (V_inv_chol_L_) V_inv_chol_L_.reset();
    if (V_inv_chol_L_qr_) V_inv_chol_L_qr_.reset();
}

void BayesianLinear::weakenInPlace(const double stdev_scale) {
    if (stdev_scale < 1)
        throw std::logic_error("weaken() called with invalid stdev multiplier " + std::to_string(stdev_scale) + " < 1");

    reset();

    if (stdev_scale == 1.0) // Nothing to do here
        return;

    if (noninf_X_) {
        // Partially informed model
        if (not noninf_X_.unique()) noninf_X_.reset(new MatrixXdR(*noninf_X_ / stdev_scale));
        else *noninf_X_ /= stdev_scale;
        if (not noninf_y_.unique()) noninf_y_.reset(new VectorXd(*noninf_y_ / stdev_scale));
        else *noninf_y_ /= stdev_scale;
    }

    if (noninformative()) return; // Nothing else to do

    double var_scale = stdev_scale*stdev_scale;
    // scale V^{-1} appropriately
    V_inv_ /= var_scale;

    // This tracks how to undo the V_inv_ weakening when calculating an updated s2 value
    pending_weakening_ *= var_scale;

    // Likewise for the Cholesky decomposition (and its inverse), if set
    if (V_chol_L_) {
        if (V_chol_L_.unique())
            *V_chol_L_ *= stdev_scale;
        else
            V_chol_L_.reset(new MatrixXd(*V_chol_L_ * stdev_scale));
    }
    if (V_inv_chol_L_) {
        if (V_inv_chol_L_.unique())
            *V_inv_chol_L_ /= stdev_scale;
        else
            V_inv_chol_L_.reset(new MatrixXd(*V_inv_chol_L_ / stdev_scale));
    }
    if (V_inv_chol_L_qr_) V_inv_chol_L_qr_.reset();

    return;
}

const MatrixXdR& BayesianLinear::noninfXData() const {
    if (not noninformative_) throw std::logic_error("noninfXData() cannot be called on a fully-informed model");

    if (not noninf_X_) const_cast<std::shared_ptr<MatrixXdR>&>(noninf_X_).reset(new MatrixXdR());

    return *noninf_X_;
}

const VectorXd& BayesianLinear::noninfYData() const {
    if (not noninformative_) throw std::logic_error("noninfYData() cannot be called on a fully-informed model");

    if (not noninf_y_) const_cast<std::shared_ptr<VectorXd>&>(noninf_y_).reset(new VectorXd());

    return *noninf_y_;
}

void BayesianLinear::reset() {
    if (last_draw_.size() > 0) last_draw_.resize(0);
    discard();
}

BayesianLinear::draw_failure::draw_failure(const std::string &what) : std::runtime_error(what) {}

BayesianLinear::draw_failure::draw_failure(const std::string &what, const BayesianLinear &model) : std::runtime_error(what + "\n" + (std::string) model) {}

}}
