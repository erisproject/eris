#pragma once
#include "Member.hpp"
#include "Agent.hpp"
#include "Firm.hpp"
#include "Good.hpp"
#include "Bundle.hpp"
#include <map>

// Base class for markets in Eris.  At this basic level, a market has an output bundle and a price
// unit bundle (typically a bundle of a single good with quantity normalized to 1).  The output
// bundle is often a single good, but could be a "bundle" of goods in the usual economic sense as
// well.  A market also has a set of suppliers (Firms) and a maximum quantity (potentially
// infinite), and (abstract) abilities to price and purchase a output quantity of an arbitrary size.
//
// Purchases are made as multiples of the output bundle, with a price determined as a multiple of
// the price unit bundle.  Thus the specific quantities in each bundle are scale invariant--only
// relative differences of quantities within a multi-good price or output bundle are relevant.  For
// ease of interpretation, it is thus recommended to normalize one of the quantities in each bundle
// to 1, particularly when the price or output bundle consists of a single good.
//
// Subclasses must, at a minimum, define the price(double q) and buy(...) methods: this abstract
// class has no allocation mechanism.

namespace eris {

class Market : public Member {
    public:
        virtual ~Market() = default;

        Market(Bundle output, Bundle priceUnit);

        // Returns the price of a multiple of the output bundle as a multiple of the price unit bundle.
        // Returns a negative value if the given quantity cannot be produced (i.e. calling buy()
        // would throw a Market::output_infeasible exception).
        virtual double price(double q) const = 0;

        // Returns the price of the given multiple of the output bundle as a Bundle.  This is
        // exactly the priceUnit times price(q)
        const Bundle priceBundle(double q) const;

        // Buys q times the output bundle for price at most pMax; removes the actual price from the
        // provided assets bundle, transferring it to the seller(s), and adds the purchased amount
        // into the assets Bundle.  This method can throw several exceptions:
        //   Market::output_infeasible -- if q*output output is not available (exceeds market
        //                                production capacity)
        //   Market::low_price -- if q*output is available, but costs more than p*price
        //   Market::insufficient_assets -- if q and p are acceptable, but assets doesn't contain the
        //                                  needed price.  (Note that this is not necessarily
        //                                  thrown if assets are less than pMax*price: the actual
        //                                  transaction price could be lower).
        virtual void buy(double q, double pMax, Bundle &assets) = 0;

        // Adds f to the firms supplying in this market
        virtual void addFirm(SharedMember<Firm> f);

        // Removes f from the firms supplying in this market
        virtual void removeFirm(eris_id_t fid);

        // Changes the market's output Bundle to the new Bundle
        virtual void setOutput(Bundle out);

        const Bundle output() const;

        // Changes the market's price basis to the new Bundle
        virtual void setPriceUnit(Bundle priceUnit);

        // Returns the Bundle of price units.  Often this will simply be a Bundle of a single good
        // (usually a money good) with a quantity of 1.
        const Bundle priceUnit() const;

        class output_infeasible : public std::exception {
            public: const char* what() const noexcept { return "Requested output not available"; }
        };
        class low_price : public std::exception {
            public: const char* what() const noexcept { return "Requested output not available for given price"; }
        };
        class insufficient_assets : public std::exception {
            public: const char* what() const noexcept { return "Assets insufficient for purchasing requested output"; }
        };

    protected:
        Bundle _output, _price;
        std::map<eris_id_t, SharedMember<Firm>> suppliers;
};


}
