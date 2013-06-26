#pragma once
#include <eris/firm/PriceFirm.hpp>
#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

using eris::firm::PriceFirm;

/** Inter-period optimizer that has firms set a price and them move it up or down by some increment
 * depending on whether the previous move increased or decreased profits.  If the previous price
 * change increased profits, the next period price moves in the same direction; if the previous move
 * decreased profits, the next period moves in the opposite direction.
 */
class PriceStepper : public Stepper {
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
         */
        PriceStepper(
                const PriceFirm &firm,
                double step = 1.0/32.0,
                int increase_count = 4
                );

    protected:
        /** Calculates whether or not the price should be increase or decreased based on what
         * happened to profits in the previous period.
         *
         * "Profits" in each period are determined from the multiple of the firm's price bundle that
         * exists in the firm's assets.  When price is a single good bundle, this is
         * straightforward; when price is a multi-good bundle, profits are determined by Bundle
         * division.  In particular, if price is 1 right shoe and 2 left shoes, assets of 50 right
         * shoes and 60 left shoes would count as the same profit as assets of 30 right shoes and 60
         * left shoes.
         *
         * If profits in a period increased from the previous period, the price for the next period
         * will change by a step in the same direction as the previous period.  If profits
         * decreased, the next period's price will change by a step in the opposite direction.  If
         * profits stay exactly the same (often at 0), prices will take a step downward, regardless
         * of the previous direction.
         */
        virtual bool should_increase() const override;

        /** Takes a step, setting the new firm price to the given multiple of the current firm
         * price.
         */
        virtual void take_step(double relative) override;

        /** Declares a dependency on the handled firm when added to the simulation.
         */
        virtual void added() override;

    private:
        const eris_id_t firm_;
        const Bundle price_;

        double prev_profit_ = 0;
        mutable double curr_profit_ = 0;
};

}}
