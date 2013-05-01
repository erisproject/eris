#pragma once
#include "Bundle.hpp"
#include <initializer_list>

namespace eris { namespace agent {

/* Abstract base class for representing a firm that uses some input to produce some output.
 */
class Firm : public Agent {
    public:
        // Returns true if the firm is able to produce the given Bundle.  Returning false thus
        // indicates that the firm either cannot produce some of the items in the Bundle, or else
        // that producing the given quantities exceeds some production limit.
        virtual bool const canProduce(Bundle const &b) = 0;

        // Returns true is the firm is able to produce positive quantities of every Good provided.
        // In other words, this checks that a) the firm produces the specified goods at all, and b)
        // that the firm hasn't already hit some capacity limit for the given goods.
        virtual bool const canProduceAny(std::initializer_list<Good> goods) = 0;

        // Tells the firm to produce the given bundle.  Returns true if the firm has produced the
        // Bundle, false otherwise.  This is essentially just like canProduce(), except that
        // if the production can be done, it is performed immediately, while canProduce() doesn't
        // actually change anything.
        virtual bool produce(Bundle const &b) = 0;

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
}

} }
