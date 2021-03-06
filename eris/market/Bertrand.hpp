#pragma once
#include <eris/Market.hpp>
#include <limits>
#include <unordered_map>

namespace eris { namespace market {

/** Bertrand market companion class intended to be used with PriceFirm.  When a buyer looks to buy,
 * firms are queried for their (constant) price for the requested quantity, and the cheapest one
 * sells the good.  If the cheapest one can't provide all of the good, it provides as much as
 * possible, then the next lowest-price firm supplies, etc.  If multiple firms have exactly the same
 * price, the quantity can either be split equally across firms (the default), or a firm can be
 * chosen randomly.
 */
class Bertrand : public Market {
    public:
        /// The default value of the constructor's randomize parameter
        static constexpr bool default_randomize = false;

        /** Constructs the market.
         *
         * \param output the output Bundle for the firm
         * \param price_unit the price basis which firms in this market accept to produce multiples
         * of the output Bundle.  Firm prices for output will be multiples of this Bundle.
         * Typically this is a single-good 'money' Bundle, but that is not required.
         * \param randomize if true, randomly selects a lowest-price firm in the event of ties; the
         * default, false, evenly divides among lowest-price firms in the event of ties.
         */
        Bertrand(Bundle output, Bundle price_unit, bool randomize = default_randomize);
        /// Returns the pricing information for purchasing q units in this market.
        virtual price_info price(double q) const override;
        /// Returns the quantity (in terms of multiples of the output Bundle) that p units of the
        /// price Bundle will purchase.
        virtual quantity_info quantity(double p) const override;
        /// Reserves q units, paying at most p_max for them.
        virtual Reservation reserve(
                SharedMember<Agent> agent,
                double q,
                double p_max = std::numeric_limits<double>::infinity()) override;
        /// Adds a firm to this market.  The Firm must be a PriceFirm object (or subclass)
        virtual void addFirm(SharedMember<Firm> f) override;

    protected:
        /** Whether to randomize when multiple firms offer the good at exactly the same (lowest)
         * price.  If true, a random firm supplies the good; if false, the spending is divided
         * equally between lowest-price firms.
         */
        bool randomize;
        /** Stores a quantity and a total price that quantity is sold for. */
        struct share {
            double q; ///< The total quantity supplied by a firm
            double p; ///< The total price to be paid to the firm for the quantity `q`
        };
        /** Allocation information. */
        struct allocation {
            /// The Market::price_info associated with the allocation
            price_info p;
            /// A map of firm id to the quantity/total price `share` value of that firm
            std::unordered_map<id_t, share> shares;
        };
        /** Calculates the allocations across firms for a purchase of q units of the good.
         * Lower-priced firms get priority, with ties as decided by the randomize parameter
         * specified during object construction.
         */
        virtual allocation allocate(double q) const;
};

} }
