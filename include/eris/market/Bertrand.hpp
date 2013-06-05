#pragma once
#include <eris/Market.hpp>
#include <eris/firm/PriceFirm.hpp>
#include <eris/Random.hpp>
#include <vector>
#include <random>

namespace eris { namespace market {

/** Bertrand market companion class intended to be used with PriceFirm.  When a buyer looks to buy,
 * firms are queried for their (constant) price for the requested quantity, and the cheapest one
 * sells the good.  If the cheapest one can't provide all of the good, it provides as much as
 * possible, then the next lowest-price firm supplies, etc.  If multiple firms have exactly the same
 * price, the quantity can either be split equally across firms (the default), or a firm can be
 * chosen randomly.
 */
class Bertrand : public Market, private Random {
    public:
        /** Constructs the market.
         *
         * \param output the output Bundle for the firm
         * \param price_unit the price basis which firms in this market accept to produce multiples
         * of the output Bundle.  Firm prices for output will be multiples of this Bundle.
         * Typically this is a single-good 'money' Bundle, but that is not required.
         * \param randomize if true, randomly selects a lowest-price firm in the event of ties; the
         * default, false, evenly divides among lowest-price firms in the event of ties.
         */
        Bertrand(Bundle output, Bundle price_unit, bool randomize = false);
        /// Returns the pricing information for purchasing q units in this market.
        virtual price_info price(double q) const override;
        /// Returns the quantity (in terms of multiples of the output Bundle) that p units of the
        /// price Bundle will purchase.
        virtual double quantity(double p) const override;
        /// Buys q units, paying at most p_max for them.
        virtual void buy(double q, BundleNegative &assets, double p_max = std::numeric_limits<double>::infinity()) override;
        /// Buys as much as possible with the provided assets bundle
        virtual double buy(BundleNegative &assets) override;
        /// Adds a firm to this market.
        virtual void addFirm(SharedMember<Firm> f) override;

    protected:
        bool randomize;
        /** Stores a quantity and a total price that quantity is sold for. */
        struct share { double q, p; };
        /** Allocation information. */
        struct allocation {
            price_info p;
            std::map<eris_id_t, share> shares;
        };
        /** Calculates the allocations across firms for a purchase of q units of the good.
         * Lower-priced firms get priority, with ties as decided by the randomize parameter
         * specified during object construction.
         */
        virtual allocation allocate(double q) const;
};

} }
