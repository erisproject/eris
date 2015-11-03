#pragma once
#include <map>
#include <eris/Consumer.hpp>

namespace eris { namespace consumer {

/** Class for a consumer whose utility is a sum of first and second order polynomials.  In
 * particular, such a consumer has a utility which is the sum of (at most) \f$1 + n +
 * \frac{n(n+1)}{2}\f$ terms: a constant; \f$n\f$ linear components (\f$a_1 g_1 + \ldots + a_n
 * g_n\f$), and \f$\frac{n(n+1)}{2}\f$ order-2 components: \f$c_1 g_1^2 + c_2 g_1 g_2 + \ldots + c_n
 * g_1 g_n + c_{n+1} g_2^2 + \ldots + c_{\frac{n(n+1)}{2}} g_n^2\f$.
 *
 * Note that there is only one term for each distinct pair of goods; in other words, there is just
 * one coefficient for \f$g_1 g_2\f$ and no separate term for \f$g_2 g_1\f$.  (Thus
 * \f$\frac{n(n+1)}{2}\f$ quadratic terms, above, instead of \f$n^2\f$.
 *
 * \sa Polynomial, for additively separable, arbitrary order polynomials.
 */
class Quadratic : public Consumer::Differentiable {
    public:
        /// Initialize with no coefficients with an optional constant offset.
        Quadratic(double offset = 0.0);
        /** Initialize with linear coefficients and (optionally) a constant offset.  %Quadratic
         * coefficients have to be set individually after construction using &coef(eris_id_t, eris_id_t).
         */
        Quadratic(std::map<eris_id_t, double> linear, double offset = 0.0);

        /// Accesses the constant term.
        double& coef();
        /// `const` accessor for the constant term.
        double coef() const;
        /** Accesses the coefficient on the linear term for good \f$g\f$.  Creates it first (set
         * to 0) if it is not yet set.
         */
        double& coef(eris_id_t g);
        /** `const` accessor for the coefficient on the linear term for good \f$g\f$.  Returns 0
         * (without creating it) if not yet set.
         */
        double coef(eris_id_t g) const;
        /** Access the coefficient on the quadratic term \f$g_1 g_2\f$.  When \f$g_1 = g_2\f$, this
         * accesses the coefficient on the squared term.  Note that `coef(a, b)` and `coef(b, a)` access
         * the same coefficient.  Creates and initializes the value to 0.0 if it does not yet exist.
         *
         * Example: `consumer.coef(good1, good2) = 2;`
         */
        double& coef(eris_id_t g1, eris_id_t g2);
        /** `const` accessor for the coefficient on quadratic term \f$g_1 g_2\f$.  Returns 0
         * (without creating the coefficient) if it does not yet exist.  Note that `coef(a, b)` and
         * `coef(b, a)` access the same coefficient.
         */
        double coef(eris_id_t g1, eris_id_t g2) const;

        /// Evaluates the utility given the current coefficients at bundle \f$b\f$.
        virtual double utility(const BundleNegative &b) const;

        /// Returns the first derivative w.r.t. good g, evaluated at bundle b.
        virtual double d(const BundleNegative &b, eris_id_t g) const;
        /// Returns the second derivative w.r.t. goods g1, g2, evaluated at bundle b.
        virtual double d2(const BundleNegative &b, eris_id_t g1, eris_id_t g2) const;
    protected:
        /// The constant offset.  \sa coef()
        double offset = 0.0;
        /// The map of coefficients on linear terms.  \sa coef(eris_id_t)
        std::map<eris_id_t, double> linear;
        /** The nested map of coefficients on quadratic terms.  \sa coef(eris_id_t, eris_id_t) Note
         * that we only store values for which the outer key is less than or equal to the inner key.
         * That is, we store `quad[3][4]` but not `quad[4][3]`.  `coef(eris_id_t, eris_id_t)` handles this
         * argument reordering so that both `coef(3, 4)` and `coef(4, 3)` access `quad[3][4]`.
         */
        std::map<eris_id_t, std::map<eris_id_t, double>> quad;
};

} }
