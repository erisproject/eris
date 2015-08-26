#pragma once
#include <eris/Bundle.hpp>
#include <eris/interopt/InterStepper.hpp>

namespace eris { class Firm; namespace firm { class PriceFirm; } }

namespace eris { namespace interopt {

/** Abstract eris::interopt::Stepper subclass for a firm that decides which direction to step based
 * on whether the direction of the previous step increased or decreased profits.
 *
 * take_step(double) must be provided by an implementing class.
 *
 * \sa Stepper
 */
class ProfitStepper : public InterStepper {
    public:
        /** Constructs a new ProfitStepper optimization object for a given Firm, with profits
         * evaluated as multiples of the given Bundle.
         *
         * The algorithm always starts out with an increase the first time optimization occurs;
         * subsequent rounds depend on the result of the previous rounds.  Each time the direction
         * changes, the step size will be halved.  When multiple steps in the same direction
         * increase profits, the step size is doubled.  If the previous change had no effect on
         * profits at all, we change direction.
         *
         * \param firm the Firm which this ProfitStepper controls
         * \param profit_basis a Bundle used to measure profits levels.  In a typical monetary economy,
         * this will typically be a Bundle consisting of a single "money" good with a quantity of 1.
         * \param initial_step see Stepper
         * \param increase_count see Stepper
         * \param period see Stepper
         */
        ProfitStepper(
                const eris::Firm &firm,
                const Bundle &profit_basis,
                double initial_step = default_initial_step,
                int increase_count = default_increase_count,
                int period = default_period
                );
        /** Constructs a new ProfitStepper optimization object for a given PriceFirm, taking the
         * price basis from the PriceFirm's initial price() value.
         */
        ProfitStepper(
                const eris::firm::PriceFirm &firm, 
                double step = 1.0/32.0,
                int increase_count = 4,
                int period = 1
                );

        /** Calculates whether or not the controlled value should be increase or decreased based on
         * what happened to profits in the previous period.
         *
         * "Profits" in each period are determined from the multiple of the price_basis Bundle that
         * exists in the firm's assets.  When price is a single good bundle, this is straightforward
         * (i.e. amount of the good); when price is a multi-good bundle, profits are determined by
         * calling Bundle::multiples to determine how many multiples of the profit_basis are
         * contained in the firm's assets.  For example, if the price basis is 1 right shoe and 2
         * left shoes, assets of 50 right shoes and 60 left shoes would count as the same profit as
         * assets of 30 right shoes and 60 left shoes: both are considered a profit of 30.
         *
         * If profits in a period increased from the previous period, the controlled value for the
         * next period will change by a step in the same direction as the previous period.  If
         * profits decreased, the next period's value will change by a step in the opposite
         * direction.  If profits stay exactly the same (often at 0), the value will take a step
         * downward, regardless of the previous direction.
         */
        virtual bool should_increase() override;

        virtual void take_step(double relative) override = 0;

    protected:
        /// Declares a dependency on the handled firm when added to the simulation
        virtual void added() override;

        /// The eris_id_t of the firm this stepper applies to.
        const eris_id_t firm_;

        /** Stores a fixed Bundle telling us what money is so that we can compare profits.  Thus
         * this stores the *initial* firm price, since that will be denominated in money.  We can't/
         * use the *current* firm price since the whole point of this InterOpt is to change that.
         */
        const Bundle profit_basis_;

    private:
        double prev_profit_ = 0;
        double curr_profit_ = 0;

};

}}
