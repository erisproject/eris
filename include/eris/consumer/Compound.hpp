#pragma once
#include <eris/Bundle.hpp>
#include <eris/Consumer.hpp>
#include <eris/types.hpp>
#include <memory>

namespace eris { namespace consumer {

/** Allows you to sum two consumer classes together as a summation.  You could, for example,
 * represent \f$ u(x,y,z) = 5 + x - x^3 + x^{1/6} y^{1/3} z^{1/2} \f$ as a CompoundSum of a
 * Polynomial and CobbDouglas utility function.
 *
 * Note that nothing in the provided Consumer classes are used other than the utility() function;
 * the Consumer objects should *not* be added to a simulation---only the final consumer (i.e. the
 * CompoundSum) should be added to the Eris simulation.
 *
 * You can chain together multiple CompoundSum and CompoundProduct consumers to construct a complex
 * mathematic operation on utility functions.  For example, to sum together 3 utility functions you
 * would use a CompoundSum(CompoundSum(c1, c2), c3).  Only the final (outermost) consumer object
 * should be added to the simulation.
 *
 * Note that a CompoundSum is *not* differentiable: if your indidividual consumer utilities are
 * differentiable, you should use CompoundSum::Differentiable instead.
 *
 * Example:
 *
 *     simulation->spawn<consumer::CompoundSum>(
 *         new consumer::Polynomial(...),
 *         new consumer::CobbDouglas(...)
 *     );
 */
class CompoundSum : public Consumer {
    public:
        /** Constructs a CompoundSum from two Consumer instance pointers.  These pointers now belong
         * to this CompoundSum object and should not be directly used again.  You may, however,
         * reuse the pointers by accessing this objects .a and .b shared pointers.
         */
        CompoundSum(Consumer *first, Consumer *second) : first(first), second(second) {}

        /** Constructs a CompoundSum from two shared pointers to Consumer instances.  The consumer
         * instances may have been previously used for another CompoundSum class.
         */
        CompoundSum(std::shared_ptr<Consumer> first, std::shared_ptr<Consumer> second) : first(first), second(second) {}

        /// Returns the sum of utilities of the two consumers
        double utility(const BundleNegative &bundle) const override {
            return first->utility(bundle) + second->utility(bundle);
        }

        class Differentiable;

        /// Shared pointer to the first Consumer object given in the constructor.
        std::shared_ptr<Consumer> first,
        /// Shared pointer to the second Consumer object given in the constructor.
            second;
};

/** This is just like Compound, but restricts the summation to a pair of Consumer::Differentiable
 * consumer utilities (instead of more general Consumer objects), retaining the differentiability.
 *
 * \sa CompoundSum
 */
class CompoundSum::Differentiable : public Consumer::Differentiable {
    public:
        /** Constructs a CompoundSum::Differentiable from two Consumer::Differentiable instance
         * pointers.  These pointers now belong to this object and should not be directly used
         * again.  You may, however, reuse the pointers by accessing this objects .a and .b shared
         * pointers.
         */
        Differentiable(Consumer::Differentiable *first, Consumer::Differentiable *second) : first(first), second(second) {}

        /** Constructs a CompoundSum::Differentiable from two shared pointers to
         * Consumer::Differentiable instances.  The consumer instances may have been used elsewhere
         * before.
         */
        Differentiable(std::shared_ptr<Consumer::Differentiable> first, std::shared_ptr<Consumer::Differentiable> second)
            : first(first), second(second) {}

        /// Returns the sum of utilities of the two consumers
        double utility(const BundleNegative &bundle) const override {
            return first->utility(bundle) + second->utility(bundle);
        }

        /// Returns the derivative, which is the sum of consumer derivatives.
        double d(const BundleNegative &bundle, eris_id_t g) const override {
            return first->d(bundle, g) + second->d(bundle, g);
        }

        /// Returns the second derivative, which is the sum of consumer second derivatives.
        double d2(const BundleNegative &bundle, eris_id_t g1, eris_id_t g2) const override {
            return first->d2(bundle, g1, g2) + second->d2(bundle, g1, g2);
        }

        /// Shared pointer to the first consumer instance given in the constructor.
        std::shared_ptr<Consumer::Differentiable> first,
        /// Shared pointer to the second consumer instance given in the constructor.
            second;
};

/** Uses two consumer classes together as a product.  You could, for example, represent \f$ u(x,y) =
 * (x+y)xyz \f$ as a CompoundProduct of a Polynomial and a CobbDouglas.  (You could also represent
 * the same utility function \f$ u(x, y) = x^2yz + xy^2z \f$ as a CompoundSum of two CobbDouglas
 * utility consumers).
 *
 * \sa CompoundSum --- all the comments there apply here as well.
 * \sa CompoundProduct::Differentiable
 */
class CompoundProduct : public Consumer {
    public:
        /** Constructs a CompoundProduct from two Consumer instance pointers.  These pointers now belong
         * to this CompoundProduct object and should not be directly used again.  You may, however,
         * reuse the pointers by accessing this objects .a and .b shared pointers.
         */
        CompoundProduct(Consumer *first, Consumer *second) : first(first), second(second) {}

        /** Constructs a CompoundProduct from two shared pointers to Consumer instances.  The
         * consumer instances may have been previously used for another CompoundProduct (or related)
         * class.
         */
        CompoundProduct(std::shared_ptr<Consumer> first, std::shared_ptr<Consumer> second) : first(first), second(second) {}

        /// Returns the product of utilities
        double utility(const BundleNegative &bundle) const override {
            double ua = first->utility(bundle);
            if (ua == 0) return ua; // Short-circuit
            return ua * second->utility(bundle);
        }

        class Differentiable;

        /// The first consumer instance given in the constructor.
        std::shared_ptr<Consumer> first,
        /// The second consumer instance given in the constructor.
            second;
};

/** Multiples together a pair of Consumer::Differentiable consumer utilities, retaining the
 * differentiability.
 *
 * \sa CompoundProduct
 * \sa CompoundSum::Differentiable
 */
class CompoundProduct::Differentiable : public Consumer::Differentiable {
    public:
        /** Constructs a CompoundProduct::Differentiable from two Consumer::Differentiable instance
         * pointers.  These pointers now belong to this object and should not be directly used
         * again.  You may, however, reuse the pointers by accessing this objects .a and .b shared
         * pointers.
         */
        Differentiable(Consumer::Differentiable *first, Consumer::Differentiable *second) : first(first), second(second) {}

        /** Constructs a CompoundProduct::Differentiable from two shared pointers to
         * Consumer::Differentiable instances.  The consumer instances may have been used elsewhere
         * before.
         */
        Differentiable(std::shared_ptr<Consumer::Differentiable> first, std::shared_ptr<Consumer::Differentiable> second) : first(first), second(second) {}

        /// Returns the product of utilities of the two consumers
        double utility(const BundleNegative &bundle) const override {
            double ua = first->utility(bundle);
            if (ua == 0) return ua; // Short-circuit
            return ua * second->utility(bundle);
        }

        /** Returns the derivative of \f$ u_1(\hdots) u_2(\hdots) \f$, which is, by the product rule,
         * \f$ \frac{\partial u_1}{\partial g} u_2 + u_1 \frac{\partial u_2}{\partial g} \f$.
         */
        double d(const BundleNegative &bundle, eris_id_t g) const override {
            double grad = 0.0;
            double aprime = first->d(bundle, g);
            if (aprime != 0) grad += aprime * second->utility(bundle);

            double bprime = second->d(bundle, g);
            if (bprime != 0) grad += bprime * first->utility(bundle);

            return grad;
        }

        /** Returns a second derivative of \f$ u_1(\hdots) u_2(\hdots) \f$, which is, by the chain
         * rule (twice applied): \f$
         * \frac{\partial^2 u_1}{\partial g_1 \partial g_2} u_2 +
         * \frac{\partial u_1}{\partial g_1} \frac{\partial u_2}{\partial g_2} +
         * \frac{\partial u_1}{\partial g_2} \frac{\partial u_2}{\partial g_1} +
         * u_1 \frac{\partial^2 u_2}{\partial g_1 \partial g_2}
         * \f$.
         */
        double d2(const BundleNegative &bundle, eris_id_t g1, eris_id_t g2) const override {
            double h = 0.0;
            double a12 = first->d2(bundle, g1, g2);
            if (a12 != 0) h += a12 * second->utility(bundle);

            double a1 = first->d(bundle, g1);
            if (a1 != 0) {
                double b2 = second->d(bundle, g2);
                h += a1 * b2;
                if (g1 == g2)
                    h += b2 * a1;
            }
            if (g1 != g2) {
                double a2 = first->d(bundle, g2);
                if (a2 != 0)
                    h += a2 * second->d(bundle, g1);
            }

            double b12 = second->d2(bundle, g1, g2);
            if (b12 != 0) h += b12 * first->utility(bundle);

            return h;
        }

        /// The first consumer instance given in the constructor.
        std::shared_ptr<Consumer::Differentiable> first,
        /// The second consumer instance given in the constructor.
            second;
};

} }
