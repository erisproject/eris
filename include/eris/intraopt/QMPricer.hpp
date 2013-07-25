#pragma once
#include <eris/algorithms.hpp>
#include <eris/IntraOptimizer.hpp>

namespace eris {
namespace market { class QMarket; }
namespace intraopt {

using eris::market::QMarket;

/** Intra-period pricing mechanism for a QMarket-based market.  This optimizer takes a configurable
 * number of steps to try to find a (roughly) correct market price.  It relies on the quantity of
 * market reservations made in other intraopt optimize() to try to determine the price that just
 * sells out the market.
 *
 * \sa eris::market::QMarket
 */
class QMPricer : public IntraOptimizer {
    public:
        /// Default number of tries if not specified in constructor
        static constexpr int default_tries = 5;
        /// Default initial step if not specified in constructor
        static constexpr double default_initial_step = Stepper::default_initial_step;
        /// Default increase_count if not specified in constructor
        static constexpr int default_increase_count = Stepper::default_increase_count;

        /** Creates a new QMarket market pricer.
         *
         * \param qm the QMarket object (or subclass) that this optimizer works on
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
                const QMarket &qm,
                int tries = default_tries,
                double initial_step = default_initial_step,
                int increase_count = default_increase_count
                );

        /// Resets the number of tries used up for this period to 0.
        virtual void initialize() override;

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
        /// The excess capacity we found in the previous iteration
        double last_excess_;
};

} }
