#pragma once
#include <eris/Agent.hpp>
#include <map>
#include <vector>
#include <functional>

namespace eris {

/** Namespace for all specific eris::Consumer implementations. */
namespace consumer{}

/** Base class for consumers, a (general) specialization of an Agent which has a utility
 * function for any Bundle of goods.
 */
class Consumer : public Agent {
    public:
        /** Returns the Consumer's utility for the given Bundle.
         * /param b a BundleNegative (or Bundle) at which to evaluate the consumer's utility.
         */
        virtual double utility(const BundleNegative &b) const = 0;

        /** Returns the Consumer's utility for the current assets Bundle.  This is identical to
         * calling consumer->utility(consumer->assets).
         */
        double currUtility() const;

        class Differentiable;
        class Simple;
};

/** Specialization of Consumer which is used for consumer instances that have analytical first and
 * second derivatives.
 */
class Consumer::Differentiable : public Consumer {
    public:
        /// Returns \f$\frac{\partial u(\mathbf{g})}{\partial g_i}\f$
        virtual double d(const BundleNegative &b, MemberID gid) const = 0;
        /// Returns \f$\frac{\partial^2 u(\mathbf{g})}{\partial g_i \partial g_j}\f$
        virtual double d2(const BundleNegative &b, MemberID g1, MemberID g2) const = 0;
        /** Returns the gradient for the given goods g evaluated at Bundle b.  These must be passed
         * separately because the Bundle need not contain quantities for all valid goods.  The
         * default implementation simply calls d() for each good, but subclasses may override that
         * behaviour (i.e. if a more efficient alternative is available).
         *
         * \param g a std::vector<id_t> of the goods for which the gradient is sought
         * \param b the BundleNegative (typically a Bundle instance) at which the gradient is to be evaluated
         */
        virtual std::map<id_t, double> gradient(const std::vector<id_t> &g, const BundleNegative &b) const;
        /** Returns the Hessian (as a two-dimensional nested std::map) for the given set of goods g
         * given a Bundle b.  The Bundle and std::vector of Good ids is specified separately because
         * the Bundle is not required to contain all of the goods at which the Hessian is to be
         * evaluated (in particular, it is free to omit 0-quantity goods).
         *
         * By default this calls h() for each good-good combination, and assumes symmetry in the
         * Hessian (thus making only \f$ \frac{g(g+1)}{2} < g^2 \f$ calls to h()).  If this isn't
         * the case (which means utility has non-continuous second derivatives), this method must be
         * overridden.  As with gradient(), this may also be overridden if a more efficient
         * calculation is available.
         *
         * \param g a std::vector<id_t> of the goods for which the hessian is sought
         * \param b the Bundle at which the hessian is to be evaluated
         */
        virtual std::map<id_t, std::map<id_t, double>> hessian(const std::vector<id_t> &g, const BundleNegative &b) const;
};

/** Very simple consumer class that takes a function (or lambda) that takes a const BundleNegative &
 * and returns a utility value.
 */
class Consumer::Simple : public Consumer {
    public:
        /** Constructs a Consumer::Simple object given a function (or lambda expression).  When
         * utility() is called for this consumer, it simply dispatches the call to the passed-in
         * u function.
         *
         * \param u a function that takes a `const BundleNegative &` and returns a double.
         */
        Simple(std::function<double(const BundleNegative &)> u) : u(u) {}
        /** Utility wrapper that simply calls the function passed-in in the constructor.
         */
        double utility(const BundleNegative &b) const { return u(b); }
    private:
        std::function<double(const BundleNegative &)> u;
};

}
