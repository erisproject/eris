#pragma once
#include <eris/Agent.hpp>
#include <eris/Bundle.hpp>
#include <algorithm>
#include <exception>
#include <initializer_list>

namespace eris {

/** Namespace for all specific eris::Firm implementations. */
namespace firm {}

/** Abstract base class for representing a firm that uses some input to supply some output.
 */
class Firm : public Agent {
    public:
        /** Returns true if the firm is able to supply the given Bundle.  Returning false thus 
         * indicates that the firm either cannot supply some of the items in the Bundle, or else
         * that producing the given quantities exceeds some production limit.  By default, this
         * checks whether assets has enough to supply the request; if not, it calls canProduce with
         * the extra amount needed to fulfill the request.  simply calls canSupplyAny, and returns
         * true of false depending on whether canSupplyAny returned a value >= 1.
        */
        virtual bool canSupply(const Bundle &b) const noexcept;

        /** Returns a non-negative double indicating whether the firm is able to supply the given
         * Bundle.  Values of 1 (or greater) indicate that the firm can supply the entire Bundle
         * (and corresponds to a true return from canSupply); 0 indicates the firm cannot supply any
         * fraction of the bundle at all; something in between indicates that the firm can supply
         * that multiple of the bundle.  This method must be provided by subclasses.
         *
         * Subclasses may, but are not required to, return values larger than 1.0 to indicate that
         * capacity beyond the Bundle quantities can be supplied.  Note, however, that a return
         * value of 1.0 DOES NOT indicate that no further amount can be supplied (though subclasses
         * may add that interpretation for specialized instances).
         */
        virtual double canSupplyAny(const Bundle &b) const noexcept;

        /** This is similar to canSupplyAny(), but only returns a true/false value indicating
         * whether the firm can supply any positive multiple of the given Bundle.  This is identical
         * in functionality to (canSupplyAny(b) > 0), but more efficient (as the calculations to
         * figure out the precise multiple supplyable are skipped).
         */
        virtual bool supplies(const Bundle &b) const noexcept;

        /** Tells the firm to supply the given Bundle.  Throws one of the following exceptions if the
         * firm cannot supply the given Bundle for some reason:
         *
         * - supply_failure --- Some supply failure not covered by the below.  All exceptions thrown
         *   by this class, including the below, inherit from this class
         * - supply_mismatch --- the firm does not supply one or more of the goods in the bundle.
         *   This is not a constraint violation: the firm, even if unconstrained, simply cannot
         *   produce some of the requested goods.
         * - production_constraint --- supplying the requested bundle exceeds the firm's capacity
         *   (e.g.  a production constraint, or some other supply limit).
         *
         * If no exception is thrown, the firm has supplied the requested bundle.
         */
        virtual void supply(const Bundle &b);

        /** This is like supply(), above, but if the firm is unable to supply the requested bundle,
         * but can supply a fraction of it, it will do so, and return the fraction supplied.
         *
         * Returns 1 if the full bundle was supplied; 0 if nothing was supplied; and something in
         * between if a fraction of the bundle was supplied.
         *
         * This method may throw a supply_mismatch exception (or a subclass thereof) in the same
         * situation supply() does, but will not throw a production_constraint or supply_failure.
         * By default, this attempts to supply from current assets, and if insufficient, calls
         * produceAny() for make up the difference.  If produceAny throws a supply_mismatch
         * exception *and* no multiple of the bundle can be provided from assets, this will rethrow
         * produceAny()'s exception.
         */
        virtual double supplyAny(const Bundle &b);

        /** An exception class that can be thrown by supply() to indicate a supply failure.
         * This may be subclassed as needed to provide for more specific supply errors.
         * \see supply_mismatch
         * \see production_constraint
         */
        class supply_failure : public std::runtime_error {
            public:
                supply_failure(std::string what);
        };
        /** Exception class indicating that the firm does not supply one of more of the goods in the
         * bundle.  This is *not* a constraint violation: throwing this exception indicates that the
         * firm, even if unconstrained, simply cannot produce some of the requested goods.
         */
        class supply_mismatch : public supply_failure {
            public:
                supply_mismatch(std::string what);
                supply_mismatch();
        };
         /** Exception class thrown when supplying the requested bundle would exceeds the firm's
          * capacity (e.g. a production constraint, or some other supply limit).
          */
        class production_constraint : public supply_failure {
            public:
                /// Constructs the exception with the specified message
                production_constraint(std::string what);
                /** Constructs the exception with a default message about a capacity constraint
                 * being violated.
                 */
                production_constraint();
        };
        /** Exception class thrown when asking a firm without instantaneous production ability to
         * produce.
         */
        class production_unavailable : public production_constraint {
            public:
                /// Constructs the exception with the specified message
                production_unavailable(std::string what);
                /** Constructs the exception with a default message about this firm having no
                 * instantaneous production ability.
                 */
                production_unavailable();
        };

    protected:
        // The following are internal methods that subclasses should provide, but should only be
        // called externally indirectly through a call to the analogous supply...() function.

        /** Returns true if the firm is able to produce the requested bundle.  By default, this
         * simply calls canProduceAny() and returns true iff it returns >= 1.0, which should be
         * sufficient in most cases.
         */
        virtual bool canProduce(const Bundle &b) const noexcept;

        /** Analogous to (and called by) canSupplyAny(), this method should return a value between 0
         * and 1 indicating the proportion of the bundle the firm can instantly produce.  By default
         * it returns 0, which should be appropriate for firms that don't instantaneously produce,
         * but will certainly need to be overridden by classes with within-period production.
         */
        virtual double canProduceAny(const Bundle &b) const noexcept;

        /** Analogous to (and called by) supplies(), this method returns true if the firm is able to
         * produce some positive quantity of each of the given Bundle.  This is equivalent to
         * (canProduceAny(b) > 0), but may be more efficient when the specific value of
         * canProduceAny() isn't needed.
         *
         * Note to subclasses: by default, this simply calls (canProduceAny(b) > 0), where the
         * default canProduceAny(b) simply returns 0.  If overriding canProduceAny(b) with more
         * complicated logic, you should also contemplate overriding produces().
         */
        virtual bool produces(const Bundle &b) const noexcept;

        /** This method is called by supply() if the current assets are insufficient to supply the
         * requested bundle.  This method must be provided by a subclass; non-production firms
         * should throw one of the following exceptions:
         *
         * - supply_mismatch --- the requested bundle is not produced by this firm
         * - production_constraint --- the requested bundle would violate a production constraint
         * - production_unavailable --- the firm does not have instantaneous production ability
         * 
         * Instantaneous production firms should, if unable to produce, throw one of the first two
         * as appropriate; non-producing firms will generally throw either the first or third.
         */
        virtual void produce(const Bundle &b) = 0;

        /** This method is called by supplyAny() if the current assets are insufficient to supply
         * the requested bundle.  By default this method just calls produce(), returning 1.0 if it
         * succeeds, 0.0 if it throws a production_constraint (or subclass thereof), and passing
         * through any other exception.
         */
        virtual double produceAny(const Bundle &b);
};

/** Abstract specialization of Firm intended for firms with no instantaneous production capacity.
 * This has optimized versions of some of Firm's methods, plus an abstract produceNext method
 * intended for inter-period production.
 */
class FirmNoProd : public Firm {
    public:
        /** Throws a Firm::production_unavailable exception if called.  FirmNoProd have no
         * instantaneous production capabilities.
         */
        virtual void produce(const Bundle &b) override;
        /// Overridden to optimize by avoiding production checks.
        virtual bool supplies(const Bundle &b) const noexcept override;
        /** Overridden to optimized by skipping production method calculations and calls.  Note that
         * unlike the Firm version of this method, this will return values greater than 1 (when
         * appropriate).
         */
        virtual double canSupplyAny(const Bundle &b) const noexcept override;

        /** Calls to ensure that there is at least the given Bundle available in assets for the next
         * period.  If current assets are sufficient, this does nothing; otherwise it calls
         * produceNext with the Bundle of quantities required to hit the target Bundle.
         *
         * \param b the Bundle of quantities needed in assets() next period.
         */
        virtual void ensureNext(const Bundle &b);

        /** Called to produce at least b for next period.  This is typically called indirectly
         * through ensureNext(), which takes into account current (undepreciated) assets to hit a
         * target capacity, which is typically updated by an optimizer such as QFStepper.
         *
         * \param b the (minimum) Bundle to be produced and added to current assets().
         *
         * \sa QFirm
         * \sa QFStepper
         */
        virtual void produceNext(const Bundle &b) = 0;
};

}
