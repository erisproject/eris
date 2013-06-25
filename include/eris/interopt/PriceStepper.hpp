#pragma once
#include <eris/firm/PriceFirm.hpp>
#include <eris/Optimizer.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

using eris::firm::PriceFirm;

/** Inter-period optimizer that has firms set a price and them move it up or down by some increment.
 * If the previous price change increased profits, the next period price moves in the same
 * direction; if the previous move decreased profits, the next period moves in the opposite
 * direction.
 */

class PriceStepper : public InterOptimizer {
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
         * \param step the initial size of a price step, relative to the current price.  Defaults to
         * 1/32 (about 3.1%).  Over time the step size will change based on the following options.
         * When increasing, the new price is \f$(1 + step)\f$ times the current price; when
         * decreasing the price the new price is \f$\frac{1}{1 + step}\f$ times the current price.
         * This ensures that an increase followed by a decrease will result in the same price.
         *
         * \param increase_count if a price moves consistently in one direction this many times, the
         * step size will be doubled.  Defaults to 3.  Must be at least 2, though 3 is typically
         * needed for decent results.  When increasing, the previous increase count is halved; thus,
         * with the default value of 3, it takes only two additional increases to trigger another
         * step increase.  With a value of 6, it would take 6 changes for the first increase, then 3
         * for subsequent increases.
         */
        PriceStepper(
                const PriceFirm &firm,
                double step = 1.0/32.0,
                int increase_count = 3
                );

        /** Calculates the price to use next period, based on what happened in the previous period,
         * the current step size, and the optimizer parameters.
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
         *
         * The step size starts out as specified in the constructor, but changes: when steps
         * oscillate between positive and negative, the step size decreases; when steps are
         * consistently in the same direction, the step size increases.
         */
        virtual void optimize() const override;

        /** Applies the price change calculated in optimize().
         */
        virtual void apply() override;
    protected:
        /** Called to change the price.  Increases or decreases it (by the current step) based on
         * calculations done in apply().  Also changes the step size (by calling change_price())
         * when appropriate, based on the sequence of past price changes.
         */
        virtual void change_price();
        /** Called to check whether the step needs to be updated, and updates it (and related
         * variables) if so.  Note that the step will never be smaller than min_step.
         */
        virtual void check_step();
        /** The minimum (relative) step size allowed: this is equal to machine epsilon (i.e. the
         * smallest value v such that 1 + v is a value distinct from 1.
         */
        const double min_step = std::numeric_limits<double>::epsilon();
    private:
        const eris_id_t firm_;
        const Bundle price_;
        int same_ = 0;
        bool prev_up_ = false;
        double step_;
        const int increase_;
        double prev_profit_ = 0;

        mutable bool curr_up_ = false;
        mutable double curr_profit_ = 0;
};

}}
