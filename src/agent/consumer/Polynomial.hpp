#pragma once
#include "agent/consumer/Consumer.hpp"
#include "Bundle.hpp"
#include "types.hpp"
#include <map>

/* Class for a simple consumer whose utility is separably additive and
 * polynomial across goods.  That is, utility is representable as: $u(X) =
 * c_0 + f(x_1) + g(x_2) + \ldots$ where f() and g() are polynomials.
 *
 * Values for coefficients, $c_1 \ldots c_n$, specify the coefficients for
 * $x_i^1 through x_i^n$.  Any constant terms must be aggregated and included
 * in c_0 (by setting obj.offset).
 */

class Polynomial : public Consumer::Differentiable {
    public:
        // Initialize with empty
        Polynomial(double offset = 0.0);
        // Initialize with map of good -> [coefs]
        Polynomial(std::map<eris_id_t, std::vector<double>> coef, double offset = 0.0);

        virtual std::vector<double> &operator[](eris_id_t gid);

        double offset = 0.0;

        virtual double utility(Bundle b);

        virtual double d(Bundle b, eris_id_t g);
        virtual double d2(Bundle b, eris_id_t g1, eris_id_t g2);
        // Override Consumer's hessian function because we can avoid the off-diagonal calculations:
        virtual std::map<eris_id_t, std::map<eris_id_t, double>> hessian(std::vector<eris_id_t> g, Bundle b);
    protected:
        std::map<eris_id_t, std::vector<double>> coef;
};
