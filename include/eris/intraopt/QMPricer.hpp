#pragma once
#include <eris/algorithms.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/market/Quantity.hpp>

namespace eris { namespace intraopt {

using eris::market::Quantity;

/** Intra-period pricing mechanism for a Quantity-based market.  This optimizer takes a configurable
 * number of steps to try to find a (roughly) correct market price.  It relies on the quantity of
 * market reservations made in other intraopt optimize() to try to determine the price that just
 * sells out the market.
 *
 * \sa eris::market::Quantity
 */
class QMPricer : public IntraOptimizer {
    public:
        /** Creates a new Quantity market pricer.
         *
         * \param qm the Quantity market this optimizer works on
         * \param tries the number of steps to take in each period.  Each step triggers a restart of
         * the optimization routine in eris::Simulation, and so making this large may be very
         * computationally costly.
         * \param step the relative step size to take when adjusting price.  \sa Stepper::Stepper
         * \param increase_count the number of consecutive changes in the same direction requires to
         * increase the step size.  \sa Stepper::Stepper
         *
         * \sa eris::Stepper
         */
        QMPricer(
                const Quantity &qm,
                int tries = 5,
                double initial_step = 1.0/32.0,
                int increase_count = 4
                );

        /// Resets the number of tries used up for this period to 0.
        virtual void initialize() override;

        /** Figures out a new price to try for this market.  If this is the first try, we take a
         * relative step (based on the current step size) up (if the market sold out) or down (if the
         * market had a surplus).
         *
         * If the most recent step got a different result than the previous (or initial) price, cut
         * the step size in half and reverse direction.  If we see `increase_count` steps in the
         * same direction, double the step size.
         *
         * If this has already been called `tries` times in this period, it does nothing and simply
         * returns false.
         */
        virtual bool postOptimize() override;

        /** Does nothing. All the action occurs in postOptimize(). */
        virtual void apply() override;

    protected:
        /// The market id this optimizer operates on.
        eris_id_t market_id_;
        /// The Stepper object used for calculating price steps
        Stepper stepper_;
        /// The number of times we adjust price each period
        int tries_ = 0;
        /// The number of times we have already tried to adjust in the current period
        int tried_ = 0;
};

} }
