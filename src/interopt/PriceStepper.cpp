#include <eris/interopt/PriceStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

PriceStepper::PriceStepper(const PriceFirm &firm, double step, int increase_count)
    : Stepper(step, increase_count), firm_(firm), price_(firm.price()) {}

bool PriceStepper::should_increase() const {
    auto firm = simAgent<PriceFirm>(firm_);
    curr_profit_ = firm->assets() / price_;

    if (stepper_.same == 0) // First time, no history to give us an informed decision; might as well increase.
        return true;
    else if (curr_profit_ > prev_profit_) // The last changed increased profit; keep going
        return stepper_.prev_up;
    else if (curr_profit_ < prev_profit_) // The last change decreased profit; turn around
        return !stepper_.prev_up;
    else // The last change did not change profit at all; lower price (this might help with some
         // perfect Bertrand cases)
        return false;
}

void PriceStepper::take_step(double relative) {
    auto firm = simAgent<PriceFirm>(firm_);

    firm->setPrice(relative * firm->price());

    prev_profit_ = curr_profit_;
}

void PriceStepper::added() {
    dependsOn(firm_);
}

} }
