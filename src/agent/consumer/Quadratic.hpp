#pragma once
#include <map>
#include "agent/consumer/Consumer.hpp"
#include "Bundle.hpp"
#include "types.hpp"

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

        // The quadratic term, however, needs to be accessed via these methods
        // (to ensure symmetry and avoid duplicate storage)
        double getQuadCoef(const eris_id_t &g1, const eris_id_t &g2);
        void setQuadCoef(const eris_id_t &g1, const eris_id_t &g2, double coef);

        virtual double utility(Bundle b);

        virtual double d(Bundle b, eris_id_t g);
        virtual double d2(Bundle b, eris_id_t g1, eris_id_t g2);
    private:
        std::map<eris_id_t, std::map<eris_id_t, double>> quad;
};
// Inline these for efficiency:
inline void Quadratic::setQuadCoef(const eris_id_t &g1, const eris_id_t &g2, double coef) {
    (g1 < g2 ? quad[g1][g2] : quad[g2][g1]) = coef;
}
inline double Quadratic::getQuadCoef(const eris_id_t &g1, const eris_id_t &g2) {
    return (g1 < g2 ? quad[g1][g2] : quad[g2][g1]);
}
