#include "eris/Consumer.hpp"

namespace eris {

double Consumer::utility() {
    return utility(assets());
}

std::map<eris_id_t, double> Consumer::Differentiable::gradient(const std::vector<eris_id_t> &goods, const BundleNegative &b) const {
    std::map<eris_id_t, double> grad;
    for (auto good : goods)
        grad[good] = d(b, good);

    return grad;
}

std::map<eris_id_t, std::map<eris_id_t, double>> Consumer::Differentiable::hessian(const std::vector<eris_id_t> &goods, const BundleNegative &b) const {
    std::map<eris_id_t, std::map<eris_id_t, double>> hess;
    std::vector<eris_id_t> priorG;

    for (auto g1 : goods) {
        for (auto g2 : priorG) {
            double hij = d2(b, g1, g2);
            hess[g1][g2] = hij;
            hess[g2][g1] = hij;
        }
        priorG.push_back(g1);
        hess[g1][g1] = d2(b,g1,g1);
    }

    return hess;
}

}
