#pragma once
#include <unordered_map>
#include <eris/Consumer.hpp>
#include <eris/Bundle.hpp>
#include <eris/types.hpp>

namespace eris { namespace consumer {

/** Class for a consumer with Cobb-Douglas utility.  Specifically any utility function matching
 * \f$ c \prod_{i=1}^{N} x_i^{\alpha_i} \f$ is supported, with \f$c, \alpha_i\f$ specifiable.  It is
 * *not* required that the \f$ \alpha_i \f$ terms sum to 1.
 *
 * Note that this class does not support negative good quantities; when evaluating utility and
 * derivatives, negative good quantities will be treated as 0.
 *
 * Negative exponents are permitted, but not particularly useful as utility will be infinite
 * whenever the good with the negative exponent is 0.
 */
class CobbDouglas : public Consumer::Differentiable {
    public:
        /** Initialize with no coefficients with an optional constant coefficient (defaulting to 1).
         * Additional coefficients can be accessed via the coef() and exp() methods.
         */
        CobbDouglas(double c = 1.0);

        /** Initialize a Cobb-Douglas utility function of up to three goods.  Additional
         * coefficients can be accessed via the coef() and exp() methods.
         */
        CobbDouglas(
                eris_id_t g1,
                double exp1,
                eris_id_t g2 = 0,
                double exp2 = 1.0,
                eris_id_t g3 = 0,
                double exp3 = 1.0,
                double c = 1.0
                );

        /** Initializes a Cobb-Douglas utility function with an arbitrary number of goods.
         *
         * \param exps a map of goods to exponents
         * \param c the optional leading coefficient, defaulting to 1.0
         */
        CobbDouglas(const std::unordered_map<eris_id_t, double> &exps, double c = 1.0);

        /// Accesses the constant term.
        double& coef();
        /// `const` accessor for the constant term.
        double coef() const;

        /** Accesses the exponent on good \f$g\f$.  Creates it (initially set to 0) if it is not yet
         * set.
         */
        double& exp(eris_id_t g);

        /** `const` accessor for the exponent on good \f$g\f$.  Returns 0 (without creating it) if
         * not yet set.
         */
        double exp(eris_id_t g) const;

        /// Evaluates the utility given the current coefficients at bundle \f$b\f$.
        virtual double utility(const BundleNegative &b) const;

        /// Returns the first derivative w.r.t. good g, evaluated at bundle b.
        virtual double d(const BundleNegative &b, eris_id_t g) const;

        /// Returns the second derivative w.r.t. goods g1, g2, evaluated at bundle b.
        virtual double d2(const BundleNegative &b, eris_id_t g1, eris_id_t g2) const;
    protected:
        /// The constant offset.  \sa coef()
        double constant = 0.0;
        /// The map of exponents.  \sa exp(eris_id_t)
        std::unordered_map<eris_id_t, double> exponents;
    private:
        // Wrapper around pow() that redefines some special 0/infinity cases.
        double power(double val, double exponent) const;
};

} }
