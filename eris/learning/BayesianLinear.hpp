#pragma once
#include <Eigen/Core>
#include <Eigen/Cholesky>
#include <memory>
#include <ostream>
#include <vector>
#include <stdexcept>
#include <string>
#include <eris/random/util.hpp>
#include <boost/random/chi_squared_distribution.hpp>

#if !defined(EIGEN_HAVE_RVALUE_REFERENCES) && !defined(EIGEN_HAS_RVALUE_REFERENCES)
// Before v3.2.7, some Eigen classes, when compiled under C++11, would get implicit move constructors
// that didn't work properly; don't even try to compile with those versions.
static_assert(false, "Eigen rvalue reference support is required (Eigen v3.2.7 or above required).")
#endif

namespace eris {
/// Namespace for classes designed for handling agent learning, beliefs and belief updating.
namespace learning {

/** Base class for a linear model with a natural conjugate, normal-gamma prior.
 */
class BayesianLinear {
    public:
        /** Default constructor: this constructor exists only to allow BayesianLinear objects to be default
         * constructed: default constructed objects are models of 0 parameters; such models will
         * throw an std::logic_error exception if any method other than copy or move assignment is
         * attempted on the object.  This primarily exists so that the following is allowed:
         *
         * BayesianLinear m;
         * ...
         * m = properly_constructed_model;
         *
         * and similar constructs where the object needs to be default constructed (such as in STL
         * containers).
         */
        BayesianLinear() = default;
        /// Default copy constructor
        BayesianLinear(const BayesianLinear &copy) = default;
        /// Default copy assignment operator
        BayesianLinear& operator=(const BayesianLinear &copy) = default;
        /// Default move constructor
        BayesianLinear(BayesianLinear &&move) = default;
        /// Default move assignment
        BayesianLinear& operator=(BayesianLinear &&move) = default;

        /** Constructs a BayesianLinear model of `K` parameters and initializes the various variables (beta,
         * s2, V, n) with highly noninformative priors; specifically this model will initialize
         * parameters with:
         *     beta = 0 vector
         *     s2 = `NONINFORMATIVE_S2` (currently 1.0)
         *     V = identity matrix times `NONINFORMATIVE_Vc` (currently 1e+8)
         *     n = `NONINFORMATIVE_N` (currently 1e-3)
         *
         * Note, however, that these values are never used: once the model is updated with enough
         * data, the above are determined entirely by the given data without incoporating the above
         * values.
         *
         * If the optional noninf_X and noninf_y arguments are provided and are non-empty matrices,
         * this stores the given data and the model becomes noninformative, but with initial data
         * that will be incorporated the next time data is added.
         *
         * The first update (via the data updating constructor) will result in a model with `n` set
         * to the number of rows in the updated data (plus noninformative data, if any) without
         * adding the initial small n value.
         *
         * noninformative() will return true, and the model cannot be used for prediction (until more data
         * is incorporated by constructor a new model with this model as a prior).
         *
         * \param K the number of model parameters
         * \param noninf_X a matrix of `K` columns of data that this model should be loaded with,
         * once enough additional data is added to make \f$X\f$ full rank.
         * \param noninf_y the y data associated with `noninf_X`
         */
        explicit BayesianLinear(
                const unsigned int K,
                Eigen::MatrixXd noninf_X = Eigen::MatrixXd(),
                Eigen::VectorXd noninf_y = Eigen::VectorXd()
                );

        /** Constructs a BayesianLinear model with the given parameters.  These parameters will be those
         * used for the prior when updating.
         *
         * \param beta the coefficient mean parameters (which, because of restrictions, might not be
         * the actual means).
         *
         * \param s2 the \f$\sigma^2\f$ value of the error term variance.  Typically the
         * \f$\sigma^2\f$ estimate.
         *
         * \param V_inverse the inverse of the model's V matrix (where \f$s^2 V\f$ is the variance
         * matrix of \f$\beta\f$).  Note: only values in the lower triangle of the matrix will be
         * used.
         *
         * \param n the number of data points supporting the other values (which can be a
         * non-integer value).
         *
         * \throws std::logic_error if any of (`K >= 1`, `V.rows() == V.cols()`, `K == V.rows()`)
         * are not satisfied (where `K` is determined by the number of rows of `beta`).
         */
        BayesianLinear(
                Eigen::VectorXd beta,
                double s2,
                Eigen::MatrixXd V_inverse,
                double n
              );

        /** Constructor for generating a posterior from a prior and a set of new data with an
         * optional weakening factor to apply to the prior.
         *
         * Typical use:
         *
         *     BayesianLinear posterior(prior, y, X);
         *
         * \param prior the BayesianLinear prior
         * \param y the new y data
         * \param X the new X data
         * \param stdev_scale the weakening factor to apply to the prior before incorporating the
         * new data.  The value must be 1.0 or above.  The value if omitted, 1.0, applies no prior
         * weakening.  The variance of the prior will be scaled by the square of this value (so that
         * the standard deviation of parameters is scaled by the given value).
         */
        BayesianLinear(
                const BayesianLinear &prior,
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                double stdev_scale = 1.0);

        /** Constructor for generating a posterior from an rvalue-prior and a set of new data and
         * optional weakening factor.  The prior's data is first moved into this object, then this
         * object is updated in-place, avoiding requiring creation of an intermediate, temporary
         * object.  Typical use:
         *
         *     BayesianLinear posterior(std::move(prior), y, X);
         *     // prior is left in a valid but indeterminate state
         *
         * or
         *
         *     // in-place weakening and update without a temporary object:
         *     belief = BayesianLinear(std::move(belief), y, X, 1.05);
         *
         * \param prior the BayesianLinear prior rvalue reference, typical the result of std::move
         * \param y the new y data.  If an empty vector, no updating is performed.
         * \param X the new X data.  The number of rows must agree with `y` and the number of
         * columns must equal `prior.K()`.
         * \param stdev_scale the weakening factor to apply to the prior before incorporating the
         * new data.  The value must be 1.0 or above.  The value if omitted, 1.0, applies no prior
         * weakening.  The variance of the prior will be scaled by the square of this value (so that
         * the standard deviation of parameters is scaled by the given value).
         */
        BayesianLinear(
                BayesianLinear &&prior,
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                double stdev_scale = 1.0);

        /** Constructor for generating a posterior by copying and weakening a prior but without any
         * data updates.  The is equivalent to calling the posterior-with-data constructor with an
         * empty set of data and the given weakening factor.
         *
         * \param prior the BayesianLinear prior
         * \param stdev_scale the weakening factor to apply to the prior.  Must be greater than or
         * equal to 1.
         */
        BayesianLinear(const BayesianLinear &prior, double stdev_scale);

        /** Constructor for generating a posterior by moving and weakening a prior but without any
         * data updates.  The is equivalent to calling the posterior-with-data moving constructor
         * with an empty set of data and the given weakening factor.
         *
         * \param prior the BayesianLinear prior rvalue reference, typical the result of std::move
         * \param stdev_scale the weakening factor to apply to the prior.  Must be greater than or
         * equal to 1.
         */
        BayesianLinear(BayesianLinear &&prior, double stdev_scale);

        /// Virtual destructor
        virtual ~BayesianLinear() = default;

        // NB: if changing these constants, also change the single-int, non-informative constructor documentation
        static constexpr double
            /** The value of `n` for a default noninformative model constructed using
             * `BayesianLinear(unsigned int)`.
             */
            //
            NONINFORMATIVE_N = 1e-3,
            /// The value of `s2` for a noninformative model constructed using `BayesianLinear(unsigned int)`
            NONINFORMATIVE_S2 = 1.0,
            /// The constant for the diagonals of the V matrix for a noninformative model
            NONINFORMATIVE_Vc = 1e+8;


        /** Virtual method called during construction to verify the model size.  If this returns a
         * non-zero value, the given parameters (beta, V for the regular constructor, K for the
         * noninformative constructor) must agree with the returned value.  If this returns 0, beta
         * and V must agree with each other, but any model size >= 1 will be accepted.
         */
        virtual unsigned int fixedModelSize() const;

        /** Accesses (computing first, if necessary) the base distribution means value of beta.
         * Note that this should not be used or reported in any significant way: it is not
         * guaranteed to be the mean of the actual distribution but is merely a parameter of the
         * distribution.  Any inference or prediction should be done by using distribution draws.
         *
         * Note also that this value is computed as needed from the \f$V^{-1}\beta\f$ and
         * \f$V^{-1}\f$ matrices, but it is those matrices, not this vector, that are used when
         * incorporating new data.
         */
        const Eigen::VectorXd& beta() const;

        /** Accesses the s2 value of the model. */
        const double& s2() const;

        /** Accesses the n value of the model. */
        const double& n() const;

        /** Accesses the \f$V^{-1}\f$ matrix of the model.
         */
        Eigen::SelfAdjointView<const Eigen::MatrixXd, Eigen::Lower> Vinv() const;

        /** Accesses (calculating if not previous calculated) the LDLT Cholesky decomposition of
         * `Vinv()`. This is essentially the same as calling `Vinv().ldlt()`, except that the
         * decomposition is stored and reused if called again. */
        const Eigen::LDLT<Eigen::MatrixXd>& VinvLDLT() const;

        /** Computes and returns an inverse of Vinv(), that is, the V matrix.  Since calculating
         * this matrix requires a numerical inverse of V, using Vinv() is preferred wherever
         * possible for both performance and precision (hence this method has the explicit name
         * `Vinvinv` rather than `V`).
         *
         * The current implementation uses VinvLDLT().solve(I), where I is an identity matrix of the
         * proper size.
         *
         * The returned value is cached once calculated to avoid calculation for subsequent calls.
         */
        const Eigen::MatrixXd& Vinvinv() const;

        /** Accesses (calculating if not previously calculated) the `L` matrix of the Cholesky
         * decomposition of the covariance matrix of beta, that is, the decomposition \f$LL^\top =
         * s^2 V\f$.  This requires both an inversion of Vinv() and then a Cholesky decomposition of
         * that inverse.
         *
         * Unfortunately, Cholesky(A^-1) != Cholesky(A)^-1, and so we have to use an actual inverse
         * here.
         *
         * Notes:
         * - Ideally I'd like something that can translate a Cholesky decomposition of a matrix to
         *   the Cholesky decomposition of the inverse; as far as I know, however, there is no such
         *   simple conversion, and so this inverse+decomposition approach has to be used.)
         * - Since we need beta(), we've already calculated VinvLDLT(), and so the inverse here
         *   (which uses the LDLT decomposition of Vinv) is quite fast compared to a "cold" inverse.
         * - Using the LLT decomposition of Vinv() would actually work for an untruncated
         *   distribution, but it fails if there are truncated regions; so we do this approach for
         *   both so that BayesianLinearRestricted and BayesianLinear and drawing using an identical
         *   covariance matrix.
         */
        Eigen::TriangularView<const Eigen::MatrixXd, Eigen::Lower> rootSigma() const;

        /** Returns the X data that has been added into this model but hasn't yet been used due to
         * it not being sufficiently large and different enough to achieve full column rank.  The
         * data will be scaled appropriately if the model is the result of a weakening/strengthening
         * of a noninformative prior, and so does not necessarily reflect the actual data added.  If
         * no data has been added, returns a null (0x0) matrix.
         *
         * \throws std::logic_error if noninformative() would return false.
         */
        const Eigen::MatrixXd& noninfXData() const;

        /** Returns the y data associated with noninfXData().  Like that data, this will be scaled
         * if the model has been weakened.
         *
         * \throws std::logic_error if noninformative() would return false.
         */
        const Eigen::VectorXd& noninfYData() const;

        /** Given a matrix of values \f$X^*\f$, predicts the mean \f$y^*\f$ by averaging draws from
         * the appropriate distribution using the current posterior.
         *
         * Each \f$y^*_i\f$ draw is from a multivariate \f$t\f$ distribution, centered at
         * \f$X\beta\f$, with variance \f$s^2{I_T + X^*\overline{V}{X^*}^\top\f$, where \f$beta\f$
         * and \f$s^2\f$ are draws from the posterior distribution, and \f$T\f$ is the number of
         * rows in \f$X^*\f$.  Finally, the \f$y^*_i\f$ draws are averaged and the resulting vector
         * is returned.
         *
         * Note that, when calling with a multi-row \f$X^*\f$, the returned \f$y^*\f$ values will be
         * appropriately correlated through the above variance matrix but the individual
         * distributions of the values will not differ.
         *
         * The drawn set of \f$\beta\f$ and \f$s^2\f$ values are cached and reused, when possible.
         * In particular, if the currently cached values contains at least the requested number of
         * draws, the first `draws` draws are used from the cached values.  If greater than the
         * cache, new draws are performed to make up the difference.  The cache can be explicitly
         * dropped (if independent predictions are desired) by calling discard().
         *
         * As a consequence, `double y1 = predict(X, 1000); double y2 = model.predict(X, 1000)` will
         * return the same predicted values of y, but `double y1 = model.predict(X, 1000);
         * model.discard(); double y2 = predict(X, 1000)` will typically produce different values of
         * `y1` and `y2`.
         *
         * If `draws` is passed with a value of 0 (the default), the current cache of draws is used,
         * whatever its size; if no draws are currently cached, this is equivalent to calling the
         * method with draws=1000.
         *
         * \throws std::logic_error if attempting to call predict() on an empty or noninformative
         * model.  (Such a model has no useful parameters).
         *
         * \param X the data rows
         * \param draws the number of draws to take (or reuse).  If 0, the current full cache of
         * draws is used, if it exists, and a draw of 1000 is used otherwise.
         * \throws std::logic_error if attempting to call predict() on an empty or noninformative
         * model.
         */
        Eigen::VectorXd predict(const Eigen::Ref<const Eigen::MatrixXd> &X, unsigned int draws = 0);

        /** This works like predict(), but it calculates both the mean and the variance of the
         * predictive values.
         *
         * \sa predict() for argument descriptions
         *
         * \returns a matrix with the first column set to the vector of predictive means as would be
         * returned by predict() and the second column set to the estimated variances of those
         * predicted values.
         */
        Eigen::MatrixXd predictVariance(const Eigen::Ref<const Eigen::MatrixXd> &X, unsigned int draws = 0);

        /** Performs predicts for one or more functions of the predicted y* values.  For example,
         * the function that simply returns y itself yields the mean; a function that returns y^2
         * can be used to obtain the prediction variance.
         *
         * \param X the data rows.
         * \param g a vector of functions to apply to y values.
         * \param draws the number of draws to take (or reuse).  If 0, the current full cache of
         * draws is used, if it exists, and a draw of 1000 is used otherwise.
         * \returns a MatrixXd with the same number of rows as X and the same number of columns as
         * the length of the `g` vector.  Each column is the mean of the associated g function
         * across draws, each row corresponds to the predicted value of y* corresponding to the
         * given row of X.
         * \throws std::logic_error if attempting to call predict() on an empty or noninformative
         * model.
         */
        virtual Eigen::MatrixXd predictGeneric(
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                const std::vector<std::function<double(double y)>> &g,
                unsigned int draws = 0);

        /** Variant of predictGeneric that takes a single function instead of a vector of functions.
         * This simply puts the function into a vector and calls predictGeneric with it. */
        Eigen::MatrixXd predictGeneric(
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                const std::function<double(double y)> &g,
                unsigned int draws = 0);

        /** Variant of predictGeneric that takes two functions instead of a vector of functions.
         * This simply puts the functions into a vector and calls predictGeneric with it. */
        Eigen::MatrixXd predictGeneric(
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                const std::function<double(double y)> &g0,
                const std::function<double(double y)> &g1,
                unsigned int draws = 0);


        /** This method explicitly discards any previously obtained beta, s2, and error draws, as
         * used by predict.  The next predict() call after a call to this method will always perform
         * new draws.
         */
        void discard();

        /** Draws a vector of \f$\beta\f$ values and \f$h^{-1} = \sigma^2\f$ values distributed
         * according to the model's parameters.  The first `K()` values are the drawn \f$\beta\f$
         * values, the last value is the drawn \f$h^{-1}\f$ value.
         *
         * In particular, this uses a gamma distribution to first draw an h value, then uses that h
         * value to draw multivariate normal beta values (conditional on h).  This means the
         * \f$\beta\f$ values will have a multivariate t distribution with mean `beta()`, covariance
         * parameter s2() times the inverse of Vinv(), and degrees of freedom parameter `n()`.
         *
         * \returns a const reference to the vector of values.  This same vector is accessible by
         * calling lastDraw().  Note that this vector is reused for subsequent draw() calls and so
         * should be copied into a new vector when storing past draws is necessary.
         *
         * Subclasses overriding this method in should also consider overriding reset().
         */
        virtual const Eigen::VectorXd& draw();

        /** Draws a multivariate normal with mean \f$\mu\f$ covariance \f$s^2LL^\top\f$ (i.e. takes
         * a constant and a Cholesky decomposition).
         *
         * \param mu the vector means
         * \param L the Cholesky decomposition matrix or matrix-like object (e.g. rootSigma())
         * \param sigma_multiplier a multiplier for the Cholesky decomposition matrix.  For example,
         * to draw from a multivariate t you could provide sqrt(n/x), where x is a chi^2(n) draw
         * (this is exactly what multivariateT() does).  Alternatively, if the desired covariance
         * equals \f$s^2 LL'\f$, this could be s (to avoid needing to multiply L).  If omitted,
         * defaults to 1 (so that you can just pass the Cholesky decomposition of the full
         * covariance matrix).
         *
         * \returns the random multivariate normal vector.
         *
         * \throws std::logic_error if mu and L have non-conforming sizes
         */
        template <typename Derived>
        static Eigen::VectorXd multivariateNormal(
                const Eigen::Ref<const Eigen::VectorXd> &mu,
                const Eigen::EigenBase<Derived> &L,
                double s = 1.0) {
            if (mu.rows() != L.rows() or L.rows() != L.cols())
                throw std::logic_error("multivariateNormal() called with non-conforming mu and L");

            // First draw a vector `z` of K N(0,s) random values:
            Eigen::VectorXd z(mu.size());
            for (int i = 0; i < z.size(); i++) z[i] = random::rnormal(0.0, s);

            // our desired multivariate normal is then just:
            return mu + L.derived() * z;
        }

        /** Draws a multivariate t with mean \f$mu\f$, variance \f$s^2LL^\top\f$ (i.e. variance is
         * taken as a constant and a Cholesky decomposition, which, when multiplied together, yield
         * \f$\Sigma\f$), and degrees of freedom nu.
         *
         * This method simply draws \f$u \sim chi^2_\nu\f$, then calls `multivariateNormal(mu, L,
         * s*sqrt(nu/u))` to return the desired multivariate T.
         *
         * \param mu the vector means
         * \param nu the degrees of freedom
         * \param L the Cholesky decomposition matrix
         * \param s a standard deviation multiplier for the Cholesky decomposition matrix.  Typically
         * a \f$\sigma\f$ (NOT \f$\sigma^2\f$) value.  If omitted, defaults to 1 (so that you can
         * just pass the Cholesky decomposition of the full covariance matrix).
         */
        template <typename Derived>
        static Eigen::VectorXd multivariateT(
                const Eigen::Ref<const Eigen::VectorXd> &mu,
                double nu,
                const Eigen::EigenBase<Derived> &L,
                double s = 1.0) {
            double u = boost::random::chi_squared_distribution<double>(nu)(random::rng());
            return multivariateNormal(mu, L, s * std::sqrt(nu/u));
        }

        /** Exception class thrown when draw() is unable to produce an admissable draw.  Not thrown
         * by this class (draws never fail) but available for subclass use.
         */
        class draw_failure : public std::runtime_error {
            public:
                /** Constructor.
                 * \param what the exception message.
                 */
                explicit draw_failure(const std::string &what);
                /** Constructor.
                 * \param what the exception message.
                 * \param model the model to append to the exception message.
                 */
                draw_failure(const std::string &what, const BayesianLinear &model);
        };


        /** Returns a reference to the vector of \f$\beta\f$ and \f$s^2\f$ values generated by the
         * last call to draw().  If draw() has not yet been called, the vector will be empty.
         */
        const Eigen::VectorXd& lastDraw() const;

        /** The number of parameters of the model, or 0 if this is not a valid model (i.e. a
         * default-constructed model).
         */
        const unsigned int& K() const { return K_; }

        /** Returns true if this model with initialized as a non-informative prior.
         *
         * \sa BayesianLinear(unsigned int)
         */
        const bool& noninformative() const;

        /** Converting a model to a string returns a human-readable model summary.
         */
        virtual operator std::string() const;

        /** Overloaded so that a BayesianLinear model can be printed nicely with `std::cout << model`.
         */
        friend std::ostream& operator << (std::ostream &os, const BayesianLinear &b);

        /** The display name of the model class to use when converting it to a summary string.
         * Called by the std::string operator when creating a model summary string.  Defaults to
         * "BayesianLinear" but subclasses should override.
         */
        virtual std::string display_name() const;

        /** Returns the parameter names.  If not set explicitly, returns a vector of {"0", "1", "2",
         * ...}.
         */
        const std::vector<std::string>& names() const;

        /** Sets the parameter names to the given vector of names.
         *
         * \param names the new names to set.  Must be either empty (to reset names to default), or
         * of length K(), to reset the current names to the given values.
         *
         * \throws std::domain_error if names is of length other than 0 or K().
         */
        void names(const std::vector<std::string> &names);

    protected:

        /** Updates beta.  This is called internally by beta() when the current beta_cache_ value is
         * empty, but can be called by other methods as well.  It resizes beta_cache_ (if necessary)
         * to size K, then populates it using Vinv() and V_inv_beta_.
         *
         * The method is const, though it does modify beta_cache_ (which is mutable).
         */
        virtual void updateBeta() const;

        /** Weakens the current linear model.  This functionality should only be used internally and
         * by subclasses as required for move and copy update methods; weakening should be
         * considered (externally) as a type of construction of a new object.
         */
        virtual void weakenInPlace(double stdev_scale);

        /** Updates the current linear model in place.  This functionality should only be used
         * internally and by subclasses as required for move and copy update methods; updating
         * should be considered (externally) as a type of construction of a new object.
         */
        virtual void updateInPlace(
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X);

        /** Called to reset any internal object state when creating a new derived object.  This is
         * called automatically at the end of weakenInPlace() and updateInPlace() (themselves called
         * when creating a new updated or weakened object), after the new object's various
         * parameters (beta_, n_, etc.) have been updated.
         *
         * The default base implementation clears the last draw value (as returned by draw() and lastDraw()),
         * and calls discard() to clear the draw cache used by predict().
         */
        virtual void reset();

        /// The model size.  If 0, this is a default-constructed object which isn't a valid model.
        unsigned int K_{0};

        /** The column vector prior of coefficient means, if calculated.  Should be reset to an
         * empty vector if it needs to be recalculated (i.e. whenever \f$V^{-1}\f$ or
         * \f$V^{-1}\beta\f$ are updated).
         */
        mutable Eigen::VectorXd beta_cache_{0};

        /// The prior value of \f$s^2\f$, the error term variance estimator
        double s2_;
        /** The underlying matrix for the V_inv_ self adjoint view.  Only the lower triangle is
         * used.
         */
        Eigen::MatrixXd V_inv_store_ = Eigen::MatrixXd::Zero(K_, K_);

        /** The \f$V^{-1} \beta\f$ vector (used when possible instead of beta, since it involves no
         * inverse).
         */
        Eigen::VectorXd V_inv_beta_ = Eigen::VectorXd::Zero(K_);

        /// The cached LDLT decomposition of V^{-1}, where LL' = V^{-1}.  \sa VinvLDLT().  Used to
        /// compute beta (from V_inv_beta_), and V_inv_inv_ (from V_inv_store_).
        mutable std::shared_ptr<Eigen::LDLT<Eigen::MatrixXd>> V_inv_ldlt_;

        /// The cached inverse of V^{-1}, i.e. V
        mutable std::shared_ptr<Eigen::MatrixXd> V_inv_inv_;

        /// The cached LLT decomposition of s2*V, where LL' = s2*V.  \sa Vinvinv()
        mutable std::shared_ptr<Eigen::LLT<Eigen::MatrixXd>> V_inv_inv_llt_;

        /// The number of data points supporting this model, which need not be an integer.
        double n_;

        /** Names associated with the beta parameters.  Uses primarily when printing the model.  If
         * unset (or has length != K()), the names are simply "0", "1", "2", ...
         */
        mutable std::shared_ptr<std::vector<std::string>> beta_names_;

        /// True if beta_names_ is simply set to the default (0 ... K-1)
        mutable bool beta_names_default_ = true;

        /** True if this model was initialized as a non-informative prior.  Any subclasses changing
         * beta/s2/etc. must take care to reset this appropriately.
         *
         * \sa BayesianLinear(unsigned int)
         */
        bool noninformative_{false};

        /** The `K+1` length vector containing the last random draw of \f$\beta\f$ and \f$s^2\f$
         * produced by the draw() method.  Will be an empty vector before the first call to draw().
         */
        Eigen::VectorXd last_draw_;

        /** The cache of drawn beta, s2, and error term values used for prediction.  Each column is
         * a draw.
         */
        Eigen::MatrixXd prediction_draws_;
        /** The cache of drawn error terms associated with prediction_draws_.  This is reused, when
         * possible, so that subsequent calls to predict() receive the same set of errors, ensuring
         * that (for example) a quadratic belief can be safely maximized since the mean every draw
         * will have had the same (constant) error mean added to it.
         */
        Eigen::MatrixXd prediction_errors_;

    private:
        // Checks that the given matrices conform; called during construction; throws on error.
        void checkLogic();

        /** Do the in-place update using the given X data when we already have an informative model.
         * This is just like updateInPlace, except it is only called after the parameters have been
         * established by having enough data to make \f$X\top X\f$ invertible.  It also doesn't
         * perform various checks on X and y are comforable.
         */
        void updateInPlaceInformative(
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X
                );

        /** The X data that has been loaded into the model but before X has full column rank.  Such
         * a model is still considered noninformative and cannot be used for results until more data
         * is added.
         *
         * If the model is weakened before becoming informative, this X data and the
         * associated y data is scaled by 1/w, where w is the weakening factor, so that \f$(X\^top
         * X)^{-1}\f$ is scaled by \f$w^2\f$, and that y data stays proportional to X.
         */
        std::shared_ptr<Eigen::MatrixXd> noninf_X_, noninf_X_unweakened_;
        /// The y data for a non-informative model.  \sa noninf_X_.
        std::shared_ptr<Eigen::VectorXd> noninf_y_, noninf_y_unweakened_;

};

}}
