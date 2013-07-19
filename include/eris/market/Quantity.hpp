#pragma once
#include <eris/types.hpp>
#include <eris/Market.hpp>
#include <limits>

namespace eris {
namespace interopt { class QMStepper; }
namespace market {

/** This class handles a "quantity" market, where at the beginning of the period firms provide a
 * fixed quantity.  In each period, the price changes based on whether there was a surplus or
 * shortage in the previous period.
 *
 * Price adjustments occur through the QMStepper inter-period optimizer class, which is
 * automatically added to a simulation when the Quantity market object is added.
 */
class Quantity : public Market {
    public:
        /** Constructs a new Quantity market, with a specified unit of output and unit of input
         * (price) per unit of output.
         *
         * \param output_unit the Bundle making up a single unit of output.  Quantities calculated
         * and exchanged in the market are multiples of this bundle.
         *
         * \param price_unit the Bundle making up a single unit of input, i.e. price.  Price values
         * calculated and exchanged in the market are multiples of this bundle.
         *
         * \param initial_price the initial per-unit price (as a multiple of price_unit) of goods in
         * this market.  Defaults to 1; must be > 0.  This is typically adjusted up or down by QMStepper (or a
         * similar inter-period optimizer) between periods.
         *
         * \param add_qmstepper if false, suppresses automatic creation of a QMStepper inter-period
         * optimizer object.  By default such an object is created when this market is added to the
         * simulation.  If overriding this, a QMStepper (or equivalent) optimizer must be added
         * separately to govern the market's price changes across periods.
         */
        Quantity(Bundle output_unit, Bundle price_unit, double initial_price = 1.0, bool add_qmstepper = true);

        /// Returns the pricing information for purchasing q units in this market.
        virtual price_info price(double q) const override;

        /** Returns the current price of 1 output unit in this market.  Note that this does not take
         * into account whether any quantity is actually available, it simply returns the going
         * price (as a multiple of price_unit) for 1 unit of the output_unit Bundle.  This typically
         * changes from one period to the next, but is constant within a period for this market
         * type.
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

        /** The ID of the automatically-created QMStepper inter-period optimizer attached to this
         * market.  Will be 0 if no such optimizer has been created.
         */
        eris_id_t optimizer = 0;

        /// Reserves q units, paying at most p_max for them.
        virtual Reservation reserve(double q, Bundle *assets, double p_max = std::numeric_limits<double>::infinity()) override;

    protected:
        /// The current price of the good as a multiple of price_unit
        double price_;

        friend class eris::interopt::QMStepper;

        /** When added to a simulation, this market automatically also adds a QMStepper inter-period
         * optimizer to handle pricing adjustments.  This can be skipped by specifying
         * add_qmstepper=false in the constructor.
         */
        virtual void added() override;

    private:
        bool add_qmstepper_ = true;

};

} }
