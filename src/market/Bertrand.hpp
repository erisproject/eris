#pragma once
#include "Market.hpp"
#include "firm/PriceFirm.hpp"
#include "Random.hpp"
#include <vector>
#include <random>

namespace eris { namespace market {

// Bertrand market companion class intended to be used with PriceFirm.  When a buyer looks to buy,
// firms are queried for their price for the requested quantity, and the cheapest one sells the
// good.  If the cheapest one can't provide all of the good, it provides as much as possible, then
// the next lowest-price firm supplies, etc.  If multiple firms have exactly the same price, the
// quantity can either be split equally across firms (the default), or a firm can be chosen
// randomly.
class Bertrand : public Market, private Random {
    public:
        // Construct the market.  Takes the output and price unit bundles, and optionally, a boolean
        // which, if true, randomly selects a lowest-price firm in the event of times (defaults to
        // false, which evenly divides among lowest-price firms).
        Bertrand(Bundle output, Bundle priceUnit, bool randomize = false);
        virtual double price(double q) const;
        virtual void buy(double q, double pMax, Bundle &assets);
        virtual void addFirm(SharedMember<Firm> f);

    protected:
        bool randomize;
        struct share { double p, q; };
        struct allocation {
            bool feasible;
            double margPrice, totalPrice;
            std::map<eris_id_t, share> shares;
        };
        virtual allocation allocate(double q) const;
};

} }
