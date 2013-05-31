#pragma once
#include <eris/Consumer.hpp>
#include <eris/Bundle.hpp>
#include <eris/types.hpp>
#include <map>

namespace eris { namespace consumer {

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

        virtual double utility(const BundleNegative &b) const override;

        virtual double d(const BundleNegative &b, const eris_id_t &g) const;
        virtual double d2(const BundleNegative &b, const eris_id_t &g1, const eris_id_t &g2) const;
        // Override Consumer's hessian function because we can avoid the off-diagonal calculations:
        virtual std::map<eris_id_t, std::map<eris_id_t, double>>
            hessian(const std::vector<eris_id_t> &g, const BundleNegative &b) const;
    protected:
        std::map<eris_id_t, std::vector<double>> coef;
};

} }
