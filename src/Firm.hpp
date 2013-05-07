#pragma once
#include "Agent.hpp"
#include "Bundle.hpp"
#include <initializer_list>

namespace eris {

/* Abstract base class for representing a firm that uses some input to produce some output.
 */
class Firm : public Agent {
    public:
        // Returns true if the firm is able to produce the given Bundle.  Returning false thus
        // indicates that the firm either cannot produce some of the items in the Bundle, or else
        // that producing the given quantities exceeds some production limit.
        virtual bool canProduce(Bundle const &b) const = 0;

        // Returns a double between 0 and 1 indicating whether the firm is able to produce the given
        // Bundle.  1 indicates the firm can produce the entire Bundle (and corresponds to a true
        // return from canProduce); 0 indicates the firm cannot produce any fraction of the bundle
        // at all; something in between indicates that the firm can produce that multiple of the
        // bundle.  By default, this method simply calls canProduce() and returns 1 or 0 depending
        // on its result; subclasses are intended to extend this.
        virtual double canProduceAny(Bundle const &b) const { return canProduce(b) ? 1.0 : 0.0; }

        // Tells the firm to produce the given bundle.  Returns true if the firm has produced the
        // Bundle, false otherwise.  This is essentially just like canProduce(), except that
        // if the production can be done, it is performed immediately, while canProduce() doesn't
        // actually change anything.
        virtual bool produce(Bundle const &b) = 0;

        // This is like produce(), above, but if the firm is unable to produce the requested bundle,
        // but can produce a fraction of it, it will do so, and return the fraction produced.
        // Returns 1 if the full bundle was produced; 0 if nothing was produced; and something in
        // between if a fraction of the bundle was produced.  By default, this simply calls
        // produce() and returns 1.0 or 0.0, but subclasses are intended to extend this.
        virtual double produceAny(Bundle const &b) { return produce(b) ? 1.0 : 0.0; }

        // Resets a firm production.  This is called when, for example, beginning a new period, to
        // reset costs, discard perishable output, depreciate capital, etc.  By default it does
        // nothing.
        virtual void advance() {}

        // Adds the given Bundle to the firm's current stock of resources.
        virtual void addResources(Bundle b) { stock += b; }

    protected:
        // The current stock of resources.  This is allowed to have negative quantities as some
        // subclass might want that, but this class as is provides no public interface for actually
        // having negative quantities.
        BundleNegative stock;
};

}
