#pragma once
#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/Firm.hpp>
#include <eris/Good.hpp>
#include <eris/Bundle.hpp>
#include <map>

namespace eris {

/** Abstract base class for markets in Eris.
 *
 * At this basic level, a market has an output bundle and a price unit bundle (typically a bundle of
 * a single good with quantity normalized to 1).  The output bundle is often a single good, but
 * could be a "bundle" of goods in the usual economic sense as well.  A market also has a set of
 * suppliers (Firms) and a maximum quantity (potentially infinite), and (abstract) abilities to
 * price and purchase a output quantity of an arbitrary size.
 *
 * Purchases are made as multiples of the output bundle, with a price determined as a multiple of
 * the price unit bundle.  Thus the specific quantities in each bundle are scale invariant--only
 * relative differences of quantities within a multi-good price or output bundle are relevant.  For
 * ease of interpretation, it is thus recommended to normalize one of the quantities in each bundle
 * to 1, particularly when the price or output bundle consists of a single good.
 *
 * Subclasses must, at a minimum, define the price(q), quantity(p), and (both) buy(...) methods:
 * this abstract class has no allocation implementation.
 */

class Market : public Member {
    public:
        virtual ~Market() = default;

        Market(Bundle output, Bundle priceUnit);

        /** The "price" of a given quantity of the output is a little bit complicated: there is, of
         * course, the total price of the output, but decisions may also depend on marginal prices.
         * Thus the following, which includes both.
         */
        struct price_info {
            /// True if the requested quantity is available.
            bool feasible;
            /// Total price 
            double total, marginal, marginalFirst;
        };

        /** Returns the price information for buying the given multiple of the output bundle.
         * Returned is a price_info struct with .feasible set to true iff the quantity can be
         * produced in this market.  If feasible, .total, .marginal, and .marginalFirst are set to
         * the total price and marginal prices of the given quantity.  .marginal is the marginal
         * price of the last infinitesimal unit (which is not necessarily the same as the marginal
         * price for the *next* unit sold, as the quantity could just hit a threshold where marginal
         * price jumps).  .marginalFirst is the marginal price of the very first (fractional) unit
         * produced.  Essentially, marginalFirst should not depend on the quantity requested (but
         * often will depend on past market purchases), and won't change until some internal economy
         * state changes (such change need not occur in this market, however: transactions in other
         * markets may affect the participating firms of this market).
         *
         * Often .marginalFirst <= .marginal will hold, but it doesn't have to: a supplier could,
         * for example, have increasing returns to scale.
         *
         * A quantity of 0 may be provided, which is interpreted specially:
         * - .feasible will be true iff there is some positive quantity available
         * - if feasible, marginal and marginalFirst will be set to the marginal cost of the first
         *   available unit, and total will be set to 0.
         *
         * In all cases, if .feasible is false, the other values should not be used.
         */
        virtual price_info price(double q) const = 0;

        /** Returns the quantity (in terms of multiples of the output Bundle) that p units of the
         * price Bundle will purchase.
         */
        virtual double quantity(double p) const = 0;

        /** Buys q times the output Bundle for price at most p_max * price Bundle.  Removes the
         * actual purchase price (which could be less than p_max * price) from the provided assets
         * bundle, transferring it to the seller(s), and adds the purchased amount into the assets
         * Bundle.
         * \throws Market::output_infeasible if q*output is not available in this market
         * \throws Market::low_price if q*output is available, but its cost would exceed p_max
         * \throws Market::insufficient_assets if q and p_max are acceptable, but the assets Bundle
         * doesn't contain the price of the output Bundle.  Note that this is not necessarily thrown
         * when assets are less than p_max*price: the actual transaction price could be low enough
         * that assets is sufficient.
         */
        virtual void buy(double q, double p_max, Bundle &assets) = 0;

        /** Buys as much quantity as can be purchased with the given assets.  Returns the multiple
         * of the output() Bundle purchased.  The assets Bundle will be reduced by the price of the
         * purchased quantity, and increased by the purchased Bundle.
         *
         * \throws Market::output_infeasible if no market output is available at all (the same
         * condition as price() returning .feasible=false).
         */
        virtual double buy(Bundle &assets) = 0;

        /** Adds f to the firms supplying in this market. */
        virtual void addFirm(SharedMember<Firm> f);

        /** Removes f from the firms supplying in this market. */
        virtual void removeFirm(eris_id_t fid);

        /** Changes the market's output Bundle to the new Bundle. */
        virtual void setOutput(Bundle out);

        /** Returns the output units of this market. */
        const Bundle output() const;

        /** Changes the market's price basis to the new Bundle. */
        virtual void setPriceUnit(Bundle priceUnit);

        /** Returns the Bundle of price units.  Often this will simply be a Bundle of a single good
         * (usually a money good) with a quantity of 1.
         */
        const Bundle priceUnit() const;

        /** Exception class thrown when a quantity that exceeds the market capacity has been
         * requested.
         */
        class output_infeasible : public std::exception {
            public: const char* what() const noexcept override { return "Requested output not available"; }
        };
        /** Exception class throw when the market price required exceeds the maximum price
         * specified.
         */
        class low_price : public std::exception {
            public: const char* what() const noexcept override { return "Requested output not available for given price"; }
        };
        /** Exception class thrown when the buyer's assets are not sufficient to purchase the
         * requested output.
         */
        class insufficient_assets : public std::exception {
            public: const char* what() const noexcept override { return "Assets insufficient for purchasing requested output"; }
        };

    protected:
        Bundle _output, _price;
        std::unordered_map<eris_id_t, SharedMember<Firm>> suppliers;
};


}
