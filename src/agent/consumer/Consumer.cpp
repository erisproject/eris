#include "agent/consumer/Consumer.hpp"

// Returns the gradient given a bundle.  By default this just calls d() for
// each good, but subclasses may override this to provide a more efficient
// implementation (if available).
std::map<eris_id_t, double> Consumer::Differentiable::gradient(std::vector<eris_id_t> goods, Bundle b) {
    std::map<eris_id_t, double> G;
    for (auto good : goods)
        G[good] = d(b, good);

    return G;
}

// Returns the Hessian given a bundle.  By default this calls h() for each
// good-good combination, but assumes symmetry in the Hessian (thus making only
// g(g+1)/2 calls instead of g^2).  If this isn't the case (which means utility
// is a very strange function), this method must be overridden.  Subclasses may
// also override if they have a more efficient means of calculating the
// Hessian.
std::map<eris_id_t, std::map<eris_id_t, double>> Consumer::Differentiable::hessian(std::vector<eris_id_t> goods, Bundle b) {
    std::map<eris_id_t, std::map<eris_id_t, double>> H;

    std::vector<eris_id_t> priorG;

    for (auto g1 : goods) {
        for (auto g2 : priorG) {
            double hij = d2(b, g1, g2);
            H[g1][g2] = hij;
            H[g2][g1] = hij;
        }
        priorG.push_back(g1);
        H[g1][g1] = d2(b,g1,g1);
    }

    return H;
}
