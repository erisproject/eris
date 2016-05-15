#pragma once
#include <eris/belief/BayesianLinear.hpp>

namespace eris { namespace belief {

/** Bayesian probit implementation for a model such as:
 *
 * \f[
 *     y^* &= X\beta + u \\
 *     y_i &= \left\{
 *      \begin{array}{ll}
 *          1 & \mbox{if } y^*_i \geq 0 \\
 *          0 & \mbox{if } y^*_i < 0
 *      \end{array}
 *      \right.
 * \f]
 * where \f$y\f$ anre \f$X\f$ are observed, but $y^*$ is an unobserved, latent variable.  As in a
 * maximum likelihood probit, the actual threshold value (0 above) is irrelevant (as long as \f$X\f$
 * contains a constant).
 *
 * This class imposes the usual probit \f$s^2 = 1.0\f$ necessary identification condition.
 */
class BayesianProbit : public BayesianLinear {
    public:
        /// Default constructor; like BayesianLinear this constructs an invalid model.
        BayesianProbit() = default;
        /// Default copy constructor
        BayesianProbit(const BayesianProbit&) = default;
        /// Default copy assignment operator
        BayesianProbit& operator=(const BayesianProbit&) = default;
        /// Default move constructor
        BayesianProbit(BayesianProbit&&) = default;
        /// Default move assignment
        BayesianProbit& operator=(BayesianProbit&&) = default;
        /** Constructs a BayesianProbit model with `K` parameters using a noninformative prior.  See
         * BayesianLinear() for details.
         */
        explicit BayesianProbit(unsigned int K);
        /** Constructs a BayesianProbit model with the given parameters.
         *
         * \param beta the coefficient mean parameters.
         * \param V_inverse the inverse of the model's V matrix (where \f$V\f$ is the variance
         * matrix of \f$\beta\f$).  Note: only the lower triangle of the matrix will be used.
         * \param n the number of data points supporting the other values (which can be a
         * non-integer value).
         *
         * \throws std::logic_error if any of (`K >= 1`, `V.rows() == V.cols()`, `K == V.rows()`)
         * are not satisfied (where `K` is determined by the number of rows of `beta`).
         */
        BayesianProbit(
                const Eigen::Ref<const Eigen::VectorXd> &beta,
                const Eigen::Ref<const Eigen::MatrixXd> &V_inverse,
                double n);

        /** Constructs a new BayesianProbit from a prior and new data.
         *
         * \param prior the prior
         * \param y the 0/1 y values.  This should only contain values equal to 0 or 1.
         * \param X the X values.
         * \param weaken if greater than 1.0, the amount to weaken before updating.
         *
         * \throws std::logic_error 
         */
        BayesianProbit(
                const BayesianProbit &prior,
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                double weaken = 1.0);

        /** Constructs a new BayesianProbit by moving an existing prior and updating it with data.
         *
         * \param prior the prior rvalue
         * \param y the 0/1 y values.  This should only contain values equal to 0 or 1, though this
         * is not (currently) checked.
         * \param X the X values.
         * \param weaken if greater than 1.0, the amount to weaken before updating.
         */
        BayesianProbit(
                BayesianProbit &&prior,
                const Eigen::Ref<const Eigen::VectorXd> &y,
                const Eigen::Ref<const Eigen::MatrixXd> &X,
                double weaken = 1.0);

        /** Constructs a new BayesianProbit by weakening a prior.
         *
         * \param prior the prior
         * \param weaken if greater than 1.0, the amount to weaken before updating.
         */
        BayesianProbit(const BayesianProbit &prior, double weaken);

        /** Constructs a new BayesianProbit by weakening a prior.
         *
         * \param prior the prior rvalue
         * \param weaken if greater than 1.0, the amount to weaken before updating.
         */
        BayesianProbit(BayesianProbit &&prior, double weaken);


};

}}
