#pragma once
#include <eris/types.hpp>
#include <eris/Market.hpp>
#include <eris/Optimizer.hpp>
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
         * \param add_qmstepper if false, suppresses automatic creation of a QMStepper inter-period
         * optimizer object.  By default such an object is created when this market is added to the
         * simulation.  If overriding this, a QMStepper (or equivalent) optimizer must be added
         * separately to govern the market's price changes across periods.
         */
        Quantity(Bundle output_unit, Bundle price_unit, bool add_qmstepper = true);

        /// Returns the pricing information for purchasing q units in this market.
        virtual price_info price(double q) const override;

        /// Returns the quantity (in terms of multiples of the output Bundle) that p units of the
        /// price Bundle will purchase.
        virtual double quantity(double p) const override;

        /// Buys q units, paying at most p_max for them.
        virtual void buy(double q, BundleNegative &assets, double p_max = std::numeric_limits<double>::infinity()) override;

        /// Buys as much as possible with the provided assets bundle
        virtual double buy(BundleNegative &assets) override;

        /** The ID of the automatically-created QMStepper inter-period optimizer attached to this
         * market.  Will be 0 if no such optimizer has been created.
         */
        eris_id_t optimizer = 0;

    protected:
        /// The current price of the good as a multiple of price_unit
        double price_;
        /// The current available quantity of the good, as a multiple of output_unit
        double quantity_;

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
