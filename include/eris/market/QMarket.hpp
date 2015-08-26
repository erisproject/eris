#pragma once
#include <eris/Optimize.hpp>
#include <eris/Market.hpp>
#include <eris/algorithms.hpp>
#include <limits>

namespace eris { namespace market {

/** This class handles a "quantity" market, where at the beginning of the period firms provide a
 * fixed quantity.  In each period, the price changes based on whether there was a surplus or
 * shortage at the previous price level.
 *
 * The class is its own intra-period pricing optimizer, taking a configurable number of steps to try
 * to find a (roughly) correct market price.  It relies on the quantity of market reservations made
 * in the intraOptimize() of other optimizers to try to determine the price that just exactly sells
 * out the market.
 */
class QMarket : public Market,
    public virtual intraopt::Initialize,
    public virtual intraopt::Reoptimize,
    public virtual intraopt::Finish {
    public:
        /// Default initial price, if not given in constructor
        static constexpr double default_initial_price = 1.0;
        /// Default repricing tries, if not given in constructor
        static constexpr unsigned int default_pricing_tries = 5;
        /// Default initial round repricing tries, if not given in constructor
        static constexpr unsigned int default_pricing_tries_first = 25;

        /** Constructs a new quantity market, with a specified unit of output and unit of input
         * (price) per unit of output.
         *
         * \param output_unit the Bundle making up a single unit of output.  Quantities calculated
         * and exchanged in the market are multiples of this bundle.
         *
         * \param price_unit the Bundle making up a single unit of input, i.e. price.  Price values
         * calculated and exchanged in the market are multiples of this bundle.
         *
         * \param initial_price the initial per-unit price (as a multiple of price_unit) of goods in
         * this market.  Defaults to 1; must be > 0.  This will be adjusted during periods to get
         * close to having no excess supply/demand for the firms' quantities, depending on the
         * following settings.
         *
         * \param pricing_tries if greater than 0, this specifies the number of tries to try a
         * price during intra-period optimization.  Values of this parameter greater than 1 mean
         * multiple prices will be tried in a period.  Each retry causes an reset of the entire
         * intraopt stage, and can thus be computationally expensive, but are needed for the market
         * to act as if governed by a sort of (imperfect) Walrasian pricer.  If 0, the market will
         * not attempt to adjust its price, in which case some external mechanism should be created
         * to manage the market's price.  Higher values give "better" prices (in the sense of being
         * closer to equilibrium), at the cost of increased computational time (since every retry
         * requires reoptimization of *every* agent).
         *
         * \param pricing_tries_first just like pricing_tries, but applies to the first optimization
         * after the market is added to a simulation.  This should typically be much higher than
         * pricing_tries to deal with the fact than an initial price is often arbitrary, while
         * prices in later periods at least have the current price (i.e. from the previous period)
         * as a useful reference point.
         */
        QMarket(Bundle output_unit,
                Bundle price_unit,
                double initial_price = default_initial_price,
                unsigned int pricing_tries = default_pricing_tries,
                unsigned int pricing_tries_first = default_pricing_tries_first);

        /// Returns the pricing information for purchasing q units in this market.
        virtual price_info price(double q) const override;

        /** Returns the current price of 1 output unit in this market.  Note that this does not take
         * into account whether any quantity is actually available, it simply returns the going
         * price (as a multiple of price_unit) for 1 unit of the output_unit Bundle.  This typically
         * changes within periods as the market tries to get close to a market clearing price.
         */
        virtual double price() const;

        /** Returns the quantity (in terms of multiples of the output Bundle) that p units of the
         * price Bundle will purchase.
         */
        virtual Market::quantity_info quantity(double p) const override;

        /** Queries the available assets of each firm participating in this market for available
         * quantities of the output bundle and returns the aggregate quantity.  If the optional
         * parameter max is specified, this will stop calculating when at least max units of output
         * are found to be available.
         *
         * Returns the quantity available if max is not specified.  If max is specified, returns a
         * value >= max when *at least* that value is available, or a value < max when the total
         * available quantity is less than max.
         *
         * Note that this simply aggregates the quantities each firm can supply; it does not combine
         * different outputs from different firms to produce a multi-good output.  For example, if
         * the output bundle is (a=1, b=1) and firm 1 has (a=10, b=2) while firm 2 has (a=3, b=6),
         * then firmQuantities() will equal 5 (2 units from firm 1, 3 from firm 2), not the 8 that
         * could be theoretically obtained with partial inputs from the two firms.
         */
        double firmQuantities(double max = std::numeric_limits<double>::infinity()) const;

        /// Reserves q units, paying at most p_max for them.
        virtual Reservation reserve(SharedMember<agent::AssetAgent> agent, double q, double p_max = std::numeric_limits<double>::infinity()) override;

        /// Adds a firm to this market.  The Firm must be a QFirm object (or subclass)
        virtual void addFirm(SharedMember<Firm> f) override;

        /** Intended to be used by WalrasianPricer (or other price-governing optimizer) to update this
         * market's price.
         */
        virtual void setPrice(double p);

        /// Resets the number of tries used up for this period to 0.
        virtual void intraInitialize() override;

        /** Figures out a new price to try for this market.  If this is the first try, we take a
         * relative step (based on the current step size) up (if the market sold out) or down (if the
         * market had a surplus).
         *
         * If the most recent step got a different result (i.e. excess capacity versus no excess
         * capacity) than the previous (or initial) price, cut the step size in half and reverse
         * direction.  If we see `increase_count` steps in the same direction, double the step size.
         *
         * If the previous step was a price decrease, but excess capacity either increased or stayed
         * the same, assume we've hit some sort of market saturation, and so increase the price
         * (contradicting the rule above, saying we should decrease it further).
         *
         * If this has already been called `tries` times in this period, it does nothing and simply
         * returns false.
         */
        virtual bool intraReoptimize() override;

        /** Resets the first-period state once a period is successfully finished.
         */
        virtual void intraFinish() override;

        /** The Stepper object used for this market.  This is public so that the stepper parameters
         * can be changed; the default stepper object uses Stepper default values, except for the
         * minimum step size which is overridden to 1/65536 (thus the minimum step size is about
         * 0.0015% of the current price)
         */
        Stepper stepper {Stepper::default_initial_step, Stepper::default_increase_count, 1.0/65536.0};

    protected:
        /// The current price of the good as a multiple of price_unit
        double price_;

        /// The number of times we adjust price each period
        unsigned int tries_;
        /** The number of times to adjust price in the *first* period.  This is usually considerably
         * larger than `tries_` so as to be allowed to break free of the given initial value.
         * Fewer tries for subsequent adjustments is justified on the grounds that variation from
         * one period to the next is likely much smaller than variation from exogenous initial
         * parameter to first period value.
         */
        unsigned int tries_first_;
        /// The number of times we have already tried to adjust in the current period
        unsigned int tried_ = 0;
        /// The excess capacity we found in the previous iteration
        double last_excess_ = 0.0;

        /// Tracks whether this is the first period or not
        bool first_period_ = true;

        /// Resets first_period_ to true when added to a simulation.
        virtual void added() override;
};

} }
