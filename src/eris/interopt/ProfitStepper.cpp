#include <eris/interopt/ProfitStepper.hpp>

namespace eris { namespace interopt {

ProfitStepper::ProfitStepper(
        const eris::Firm &firm,
        const Bundle &profit_basis,
        double step,
        int increase_count,
        int period
        ) : InterStepper(step, increase_count, period), firm_(firm), profit_basis_(profit_basis) {}

ProfitStepper::ProfitStepper(
        const eris::firm::PriceFirm &firm, 
        double step,
        int increase_count,
        int period
        ) : ProfitStepper(firm, firm.price(), step, increase_count, period) {}

bool ProfitStepper::should_increase() const {
    auto firm = simAgent<Firm>(firm_);
    curr_profit_ = firm->assets().multiples(profit_basis_);

    bool incr = false;

    if (stepper_.same == 0) // First time (or just after a jump), no history to give us an informed
        incr = true;        // decision; might as well increase.
    else if (curr_profit_ > prev_profit_) // The last changed increased profit; keep going
        incr = stepper_.prev_up;
    else if (curr_profit_ < prev_profit_) // The last change decreased profit; turn around
        incr = not stepper_.prev_up;
    else // The last change did not change profit at all; lower price (this might help with some
         // perfect Bertrand cases)
        incr = false;

    prev_profit_ = curr_profit_;
    return incr;
}

void ProfitStepper::added() {
    dependsOn(firm_);
}

}}
