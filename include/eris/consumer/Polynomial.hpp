#pragma once
#include <eris/Consumer.hpp>
#include <eris/Bundle.hpp>
#include <eris/types.hpp>
#include <map>
#include <vector>

namespace eris { namespace consumer {

/** Consumer implementation for a consumer whose utility is separably additive and polynomial across
 * goods.  That is, utility is representable as: \f$u(\mathbf{X}) = c_0 + f(X_1) + g(X_2) +
 * \ldots\f$, where \f$f(\cdot)\f$ and \f$g(\cdot)\f$ are polynomials.
 */
class Polynomial : public Consumer::Differentiable {
    public:
        /** Initialize with just a constant, which defaults to 0.  Until coefficients are updated,
         * the consumer's utility will be this constant for any Bundle of goods.
         */
        Polynomial(double offset = 0.0);
        /** Initialize with a map of good ID => vector of coefficients, and optionally a constant
         * offset.  The order of coefficients is in increasing order, beginning at the coefficient
         * on the linear term, then the squared term, etc.
         *
         * For example, initializing with a map of \f$\{1 \to [5,-1], 3 \to [0,5,-1]\}\f$ and
         * constant of 3 creates a consumer with utility function
         * \f[
         *     u(\mathbf{X}) = 3 + 5 x_1 - x_1^2 + 5 x_3^2 - x_3^3
         * \f]
         */
        Polynomial(std::map<eris_id_t, std::vector<double>> coef, double offset = 0.0);

        /** Returns a (mutable) reference to coefficient \f$n\f$ on good \f$g\f$ (i.e. the
         * coefficient on the \f$g^{n+1}\f$ term).  If the coefficient or lesser coefficients for
         * the given good do not yet exist, they will be automatically created with values of 0.
         *
         * The coefficients std::map can also be accessed directly via the coefficients member, but
         * such access does not include any auto-vivification.
         */
        virtual double& coef(const eris_id_t &g, const int &n);

        /** `const` version of `coef` that does not auto-vivify elements, but still returns 0 when
         * accessing non-existing elements.
         */
        virtual double coef(const eris_id_t &g, const int &n) const;

        /** Returns a reference to the constant offset term.  Identical to accessing the `offset`
         * member directly.
         */
        virtual double& coef();

        /// `const` version of `coef()`.
        virtual double coef() const;

        /** Calculates and returns the consumer's utility for the given bundle, using the offset and
         * coefficients currently assigned.
         */
        virtual double utility(const BundleNegative &b) const override;

        /** Returns the first derivative of utility with respect to good \f$g\f$, evaluated at
         * Bundle \f$b\f$.  Mathematically:
         * \f[
         *      \frac{\partial u(b)}{\partial g}
         * \f]
         */
        virtual double d(const BundleNegative &b, const eris_id_t &g) const;
        /** Returns the second derivate with respect to \f$g_1\f$ then \f$g_2\f$, evaluated at
         * Bundle \f$b\f$.  Mathematically:
         * \f[
         *      \frac{\partial^2 u(b)}{\partial g_1 \partial g_2}
         * \f]
         * Note that for the utility functions implementable by this class the second derivative is
         * 0 whenever \f$g_1 \neq g_2\f$.
         */
        virtual double d2(const BundleNegative &b, const eris_id_t &g1, const eris_id_t &g2) const;
        /** Returns the consumer's hessian.  This overrides the implementation in
         * Consumer::Differentiable with a more efficient version, since only diagonal elements need
         * to be calculated (off-diagonal elements are always 0).
         */
        virtual std::map<eris_id_t, std::map<eris_id_t, double>>
            hessian(const std::vector<eris_id_t> &g, const BundleNegative &b) const;

    protected:
        /// The constant offset term in the consumer's utility.
        double offset = 0.0;

        /// The map of coefficients for the consumer's utility.
        std::map<eris_id_t, std::vector<double>> coefficients;

};

} }
