#pragma once
#include "Agent.hpp"
#include "Bundle.hpp"
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
        // simply calls canSupplyAny, and returns true of false depending on whether canSupplyAny
        // returned a value >= 1.
        virtual bool canSupply(Bundle const &b) const { return canSupplyAny(b) >= 1.0; }

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
        virtual double canSupplyAny(Bundle const &b) const = 0;

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
        virtual void supply(Bundle const &b) = 0;

        // This is like supply(), above, but if the firm is unable to supply the requested bundle,
        // but can supply a fraction of it, it will do so, and return the fraction supplied.
        // Returns 1 if the full bundle was supplied; 0 if nothing was supplied; and something in
        // between if a fraction of the bundle was supplied.  This method may throw a
        // supply_mismatch exception (or a subclass thereof) in the same situation supply() does,
        // but will not throw a supply_constraint or supply_failure.  By default, this simply calls
        // supply() returning 1.0 if it succeeded and 0.0 if it throws a supply_failure exception
        // other than supply_mismatch.  Subclasses are intended to extend this in most cases,
        // especially if canSupplyAny returns values between 0 and 1.
        virtual double supplyAny(Bundle const &b) {
            try { supply(b); }
            catch (supply_mismatch &e) { throw e; }
            catch (supply_failure&) { return 0.0; }
            return 1.0;
        }

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
};

}
