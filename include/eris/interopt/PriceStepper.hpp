#pragma once
#include <eris/firm/PriceFirm.hpp>
#include <eris/interopt/ProfitStepper.hpp>

namespace eris { namespace interopt {

using eris::firm::PriceFirm;

/** Inter-period optimizer that has firms set a price and them move it up or down by some increment
 * depending on whether the previous move increased or decreased profits.  If the previous price
 * change increased profits, the next period price moves in the same direction; if the previous move
 * decreased profits, the next period moves in the opposite direction.
 */
class PriceStepper : public ProfitStepper {
    public:
        /** Constructs a new PriceStepper optimization object for a given PriceFirm.  The algorithm
         * always starts with a price increase the first time optimization occurs; subsequent rounds
         * depend on the result of the previous rounds.  In particular, each time the price change
         * direction changes, the step size will be halved.  When multiple steps in the same
         * direction increase profits, the step size is doubled.
         *
         * \param firm the PriceFirm object (or subclass thereof) that this optimizer works on.  The
         * PriceFirm must already have an initial price set.
         *
         * \param step the initial step size
         *
         * \param increase_count the number of steps in the same direction required before the step
         * size is increased.
         */
        PriceStepper(
                const PriceFirm &firm,
                double step = default_initial_step,
                int increase_count = default_increase_count
                );

    protected:
        /** Takes a step, setting the new firm price to the given multiple of the current firm
         * price.
         */
        virtual void take_step(double relative) override;
};

}}
