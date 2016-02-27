#include <eris/belief/BayesianProbit.hpp>

namespace eris { namespace belief {

BayesianProbit::BayesianProbit(unsigned int K) : BayesianLinear(K) {}

BayesianProbit::BayesianProbit(
                const Eigen::Ref<const Eigen::VectorXd> &beta,
                const Eigen::Ref<const Eigen::MatrixXd> &V_inverse,
                double n)
    : BayesianLinear(beta, 1.0, V_inverse, n) {}

BayesianProbit::BayesianProbit(
        const BayesianProbit &prior,
        const Eigen::Ref<const Eigen::VectorXd> &y,
        const Eigen::Ref<const Eigen::MatrixXd> &X,
        double weaken) : BayesianProbit(prior) {
    if (weaken != 1.0) weakenInPlace(weaken);
    updateInPlace(y, X);
}

BayesianProbit::BayesianProbit(
        BayesianProbit &&prior,
        const Eigen::Ref<const Eigen::VectorXd> &y,
        const Eigen::Ref<const Eigen::MatrixXd> &X,
        double weaken) : BayesianProbit(std::move(prior)) {
    if (weaken != 1.0) weakenInPlace(weaken);
    updateInPlace(y, X);
}

BayesianProbit::BayesianProbit(const BayesianProbit &prior, double weaken) : BayesianProbit(prior) {
    weakenInPlace(weaken);
}

BayesianProbit::BayesianProbit(BayesianProbit &&prior, double weaken) : BayesianProbit(std::move(prior)) {
    weakenInPlace(weaken);
}

}}
