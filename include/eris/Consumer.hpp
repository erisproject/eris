#pragma once
#include <eris/types.hpp>
#include <eris/Agent.hpp>
#include <eris/Bundle.hpp>
#include <map>
#include <vector>

namespace eris {

/* Base class for consumers, a (general) specialization of an Agent which has a
 * utility function for any Bundle of goods.
 *
 * There is also the further specialization base class
 * Consumer::Differentiable, which is used for consumers that also have
 * analytical first and second derivatives.
 */

class Consumer : public Agent {
    public:
        virtual double utility(const Bundle &b) const = 0;

        class Differentiable;
};

class Consumer::Differentiable : public Consumer {
    public:
        // Returns $\frac{\partial u(b)}{\partial g}$
        virtual double d(const Bundle &b, const eris_id_t &gid) const = 0;
        // Returns $\frac{\partial^2 u(b)}{\partial g_1 \partial g_2}$
        virtual double d2(const Bundle &b, const eris_id_t &g1, const eris_id_t &g2) const = 0;
        // Returns the gradient for the given goods given a bundle.  (We can't just use the bundle's
        // included goods because it might not include some goods for which the gradient is needed).
        virtual std::map<eris_id_t, double> gradient(const std::vector<eris_id_t> &g, const Bundle &b) const;
        // Returns the Hessian for the given goods given a bundle.
        virtual std::map<eris_id_t, std::map<eris_id_t, double>> hessian(const std::vector<eris_id_t> &g, const Bundle &b) const;
};

// Returns the gradient given a bundle.  By default this just calls d() for each good, but
// subclasses may override this to provide a more efficient implementation (if available).
inline std::map<eris_id_t, double> Consumer::Differentiable::gradient(const std::vector<eris_id_t> &goods, const Bundle &b) const {
    std::map<eris_id_t, double> grad;
    for (auto good : goods)
        grad[good] = d(b, good);

    return grad;
}

// Returns the Hessian given a bundle.  By default this calls h() for each good-good combination,
// but assumes symmetry in the Hessian (thus making only g(g+1)/2 calls instead of g^2).  If this
// isn't the case (which means utility is a very strange function), this method must be overridden.
// Subclasses may also override if they have a more efficient means of calculating the Hessian.
inline std::map<eris_id_t, std::map<eris_id_t, double>> Consumer::Differentiable::hessian(const std::vector<eris_id_t> &goods, const Bundle &b) const {
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
