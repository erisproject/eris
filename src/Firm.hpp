#pragma once
#include "Agent.hpp"
#include "Bundle.hpp"
#include <algorithm>
#include <exception>
#include <initializer_list>

namespace eris {

/* Abstract base class for representing a firm that uses some input to supply some output.
 */
class Firm : public Agent {
    public:
        // Returns true if the firm is able to supply the given Bundle.  Returning false thus
        // indicates that the firm either cannot supply some of the items in the Bundle, or else
        // that producing the given quantities exceeds some production limit.  By default, this
        // checks whether stock has enough to supply the request; if not, it calls canProduce with
        // the extra amount needed to fulfill the request.
        // simply calls canSupplyAny, and returns true of false depending on whether canSupplyAny
        // returned a value >= 1.
        virtual bool canSupply(const Bundle &b) const noexcept { return canSupplyAny(b) >= 1.0; }

        // Returns a non-negative double indicating whether the firm is able to supply the given
        // Bundle.  Values of 1 (or greater) indicate that the firm can supply the entire Bundle
        // (and corresponds to a true return from canSupply); 0 indicates the firm cannot supply any
        // fraction of the bundle at all; something in between indicates that the firm can supply
        // that multiple of the bundle.  This method must be provided by subclasses.
        //
        // Subclasses may, but are not required to, return values larger than 1.0 to indicate that
        // capacity beyond the Bundle quantities can be supplied.  Note, however, that a return
        // value of 1.0 DOES NOT indicate that no further amount can be supplied (though subclasses
        // may add that interpretation for specialized instances).
        virtual double canSupplyAny(const Bundle &b) const noexcept;

        // Tells the firm to supply the given bundle.  Throws one of the following exceptions if the
        // firm cannot supply the given bundle for some reason:
        //   supply_failure    -- Some supply failure not covered by the below.  All exceptions
        //                        thrown by this class, including the below, inherit from this class
        //   supply_mismatch   -- the firm does not supply one or more of the goods in the bundle.
        //                        This is not a constraint violation: the firm, even if
        //                        unconstrained, simply cannot produce some of the requested goods.
        //   supply_constraint -- supplying the requested bundle exceeds the firm's capacity (e.g.
        //                        a production constraint, or some other supply limit).
        // If no exception is thrown, the firm has supplied the requested bundle.
        virtual void supply(const Bundle &b);

        // This is like supply(), above, but if the firm is unable to supply the requested bundle,
        // but can supply a fraction of it, it will do so, and return the fraction supplied.
        // Returns 1 if the full bundle was supplied; 0 if nothing was supplied; and something in
        // between if a fraction of the bundle was supplied.  This method may throw a
        // supply_mismatch exception (or a subclass thereof) in the same situation supply() does,
        // but will not throw a supply_constraint or supply_failure.  By default, this attempts to
        // supply from current stock, and if insufficient, calls produceAny() for make up the
        // difference.  If produceAny throws a supply_mismatch exception *and* no multiple of the
        // bundle can be provided from stock, this will rethrow produceAny()'s exception.
        virtual double supplyAny(const Bundle &b);

        // Resets a firm production.  This is called when, for example, beginning a new period, to
        // reset costs, discard perishable output, depreciate capital, etc.  By default it does
        // nothing.
        virtual void advance() {}

        // Adds the given Bundle to the firm's current stock of resources.
        virtual void addResources(Bundle b) { stock += b; }

        // The exceptions that can be thrown by supply().  Subclasses may define additional
        // exceptions, but they should inherit from one of these.
        class supply_failure : public std::runtime_error {
            public:
                supply_failure(std::string what) : std::runtime_error(what) {}
        };
        class supply_mismatch : public supply_failure {
            public:
                supply_mismatch(std::string what) : supply_failure(what) {}
                supply_mismatch() : supply_failure("Firm does not supply requested goods") {}
        };
        class supply_constraint : public supply_failure {
            public:
                supply_constraint(std::string what) : supply_failure(what) {}
                supply_constraint() : supply_failure("Firm cannot supply requested bundle: capacity constraint would be violated") {}
        };
    protected:
        // The current stock of resources.
        //
        // Subclasses don't have to use this at all, but could, for example, record surplus
        // production here (for example, when a firm supplys 2 units of A for each unit of B, but
        // is instructed to supply 3 units of each (thus resulting in 3 surplus units of A).
        Bundle stock;

        // The following are internal methods that subclasses should provide, but should only be
        // called externally indirectly through a call to the analogous supply...() function.

        // Returns true if the firm is able to produce the requested bundle.  By default, this
        // simply calls canProduceAny() and returns true iff it returns >= 1.0, which should be
        // sufficient in most cases.
        virtual bool canProduce(const Bundle &b) const noexcept { return canProduceAny(b) >= 1.0; }

        // Analogous to (and called by) canSupplyAny(), this method should return a value between 0
        // and 1 indicating the proportion of the bundle the firm can instantly produce.  By default
        // it returns 0, which should be appropriate for firms that don't instantaneously produce.
        virtual double canProduceAny(const Bundle &b) const noexcept { return 0.0; }

        // See supply().  This method is called by supply if the current stock is insufficient to
        // supply the requested bundle.  This method must be provided by a subclass; non-production
        // firms should throw either a supply_mismatch (the requested good is never supplied by this
        // firm) or supply_constraint (the current stock is out, and the firm cannot produce more
        // (or cannot produce at all)) exception.  Instantaneous production firms should, if unable
        // to produce, throw one of these or some other supply_failure exception (or subclass
        // thereof) as appropriate.
        virtual void produce(const Bundle &b) = 0;

        // See supplyAny().  This method is called by supplyAny() if the current stock is
        // insufficient to supply the requested bundle.  By default this method just calls
        // produce(), returning 1.0 if it succeeds, 0.0 if it throws a supply_constraint, and
        // passing through any other exception.
        virtual double produceAny(const Bundle &b) {
            try { produce(b); }
            catch (supply_constraint&) { return 0.0; }
            return 1.0;
        }
};

inline double Firm::canSupplyAny(const Bundle &b) const noexcept {
    // We can supply the entire thing from current stock:
    if (stock >= b) return 1.0;

    // Otherwise try production to make up the difference
    Bundle onhand = Bundle::common(stock, b);
    Bundle need = b - onhand;
    double c = canProduceAny(need);
    if (c >= 1.0) return 1.0;
    if (c <= 0.0) return onhand / b;

    // We can produce some, but not enough; figure out how much we can supply in total:
    need *= c;
    return (need + onhand) / b;
}

inline void Firm::supply(const Bundle &b) {
    // Check to see if we have enough stock to cover the demand
    Bundle onhand = Bundle::common(stock, b);
    Bundle need = b - onhand;

    // If needed, produce what we can't supply from stock
    if (need != 0) produce(need);

    // If we survived this far, either stock is enough or production succeeded; take the onhand
    // amount out of stock as well.
    stock -= onhand;
}

inline double Firm::supplyAny(const Bundle &b) {
    // Check to see if we have enough stock to cover the demand
    Bundle onhand = Bundle::common(stock, b);
    Bundle need = b - onhand;

    if (need == 0) {
        // Supply it all from stock
        stock -= onhand;
        return 1.0;
    }

    // Otherwise produce what we can't supply from stock
    // (NB: can't reduce stock yet, because produceAny might throw a supply_mismatch)
    double c = 0.0;
    try {
        c = produceAny(need);
    }
    catch (supply_mismatch &e) {
        if (!(onhand > 0)) {
            // We can't supply any, and can't produce any, and produceAny says it's a
            // supply_mismatch, so it really is a supply_mismatch.
            throw e;
        }
        // Otherwise ignore the exception: it isn't actually a supply mismatch since we have
        // some positive supply from surplus.
    }

    if (c > 0) {
        // produceAny() produced at least some fraction of what we need; add it to the onhand quantity.
        if (c < 1.0) need *= c;
        onhand += need;
    }
    // else produceAny() didn't produce anything

    // Add any onhand surplus back into stock
    stock -= onhand % b;
    return c >= 1.0 ? 1.0 : onhand / b;
}

}
