#pragma once
#include "Member.hpp"
#include "Agent.hpp"
#include "Firm.hpp"
#include "Good.hpp"
#include "Bundle.hpp"
#include <map>

// Base class for markets in Eris.  At this basic level, a market has an output bundle and a price
// normalization bundle (typically a bundle of a single good with quantity normalized to 1).  The
// output bundle is often a single good, but could be a "bundle" of goods in the usual economic
// sense as well.  A market also has a set of suppliers (Firms) and a maximum quantity (potentially
// infinite), and (abstract) abilities to price and purchase a output quantity of an arbitrary size.
//
// Purchases are made as multiples of the output bundle, with a price determined as a multiple of
// the input bundle.  Thus the specific quantities in each bundle are scale invariant--only relative
// differences of quantities within a multi-good price or output bundle are relevant.  For ease of
// interpretation, it is thus recommended to normalize one of the quantities in each bundle to 1,
// particularly when the price or output bundle consists of a single good.
//
// Subclasses must, at a minimum, define the price(double q) and buy(...) methods: this abstract
// class has no allocation mechanism.

namespace eris {

class Market : public Member {
    public:
        virtual ~Market() = default;

        Market(Bundle output, Bundle price);

        // Returns the price of a multiple of the output bundle as a multiple of the price bundle
        virtual double price(double q) = 0;

        // Buys q times the output bundle for price at most p; removes the actual price from the
        // provided assets bundle, transferring it to the seller(s).  This method can throw several
        // exceptions:
        //   Market::output_NA -- if q*output output is not available (exceeds market production capacity)
        //   Market::low_price -- if q*output is available, but costs more than p*price
        //   Market::insufficient -- if q and p are acceptable, but assets doesn't contain the
        //                           needed price.  (Note that this is not necessarily p*price: the
        //                           actual transaction price could be lower).
        virtual double buy(double q, double p, const Bundle &assets) = 0;

        // Adds f to the firms supplying in this market
        virtual void addFirm(SharedMember<Firm> f);

        // Removes f from the firms supplying in this market
        virtual void removeFirm(eris_id_t fid);


    protected:
        Bundle _output, _price;
        std::map<eris_id_t, SharedMember<Firm>> suppliers;
};


}
