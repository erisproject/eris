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
 * where \f$y\f$ anre \f$X\f$ are observed, but $y^*$ is an unobserved, latent variable.
 *
 * This class extends BayesianLinear, but imposes that `s2 = 1.0` always (as a necessary
 * identification condition), and updates in a very different way.
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
        /** Constructs a BayesianProbit model with the given parameters.  See the equivalent
         * BayesianLinear() for details, but note that its s2 value is always specified as 1.0.
         */
        BayesianProbit(
                const Eigen::Ref<const Eigen::VectorXd> &beta,
                const Eigen::Ref<const Eigen::MatrixXd> &V_inverse,
                double n);

        /** Constructs a new BayesianProbit from a prior and new data.
         *
         * \param prior the prior
         * \param y the 0/1 y values.  This should only contain values equal to 0 or 1, though this
         * is not (currently) checked.
         * \param X the X values.
         * \param weaken if greater than 1.0, the amount to weaken before updating.
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
