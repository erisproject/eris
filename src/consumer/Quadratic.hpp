#pragma once
#include <map>
#include <utility>
#include "consumer/Consumer.hpp"
#include "Bundle.hpp"
#include "types.hpp"

namespace eris { namespace consumer {

/* Class for a consumer whose utility is a sum of first and second order
 * polynomials.  In particular, such a consumer has a utility which is the sum
 * of (at most) $1 + n + \frac{n(n+1)}{2}$ terms: a constant; $n$ linear
 * components ($a_1 g_1 + \ldots + a_n g_n$), and $\frac{n(n+1)}{2}$ order-2
 * components: ($c_1 g_1^2 + c_2 g_1 g_2 + \ldots + c_n g_1 g_n + c_{n+1} g_2^2
 * + \ldots + c_{\frac{n(n+1)}{2}} g_n^2$
 *
 * See also: Polynomial, for additively separable, arbitrary order polynomials.
 */
class Quadratic : public Consumer::Differentiable {
    public:
        // Initialize with 0 for every coefficient
        Quadratic();
        // Initialize with constant and linear coefficients.  Quadratic coefficients have to be
        // set individually using setQuadCoef()
        Quadratic(double offset, std::map<eris_id_t, double> linear);

        // Offset and linear may be accessed directly
        double offset = 0.0;
        std::map<eris_id_t, double> linear;

        // The quadratic term, however, needs to be accessed via these methods (to ensure symmetry
        // and avoid duplicate storage)
        double getQuadCoef(eris_id_t g1, eris_id_t g2) const;
        void setQuadCoef(eris_id_t g1, eris_id_t g2, const double &coef);

        virtual double utility(const Bundle &b) const;

        virtual double d(const Bundle &b, const eris_id_t &g) const;
        virtual double d2(const Bundle &b, const eris_id_t &g1, const eris_id_t &g2) const;
    private:
        std::map<eris_id_t, std::map<eris_id_t, double>> quad;
};
// Inline these for efficiency:
inline void Quadratic::setQuadCoef(eris_id_t g1, eris_id_t g2, const double &coef) {
    if (g1 > g2) std::swap(g1, g2);
    quad[g1][g2] = coef;
}
inline double Quadratic::getQuadCoef(eris_id_t g1, eris_id_t g2) const {
    if (g1 > g2) std::swap(g1, g2);

    if (quad.count(g1) == 0) return 0.0;
    auto nested = quad.at(g1);
    if (nested.count(g2) == 0) return 0.0;
    return nested.at(g2);
}

} }
