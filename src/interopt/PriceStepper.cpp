#include <eris/interopt/PriceStepper.hpp>

namespace eris { namespace interopt {

PriceStepper::PriceStepper(const PriceFirm &firm, double step, int increase_count)
    : firm_(firm), price_(firm.price()), step_(step), increase_(increase_count) {}

void PriceStepper::optimize() const {
    SharedMember<PriceFirm> firm = simulation()->agent(firm_);
    curr_profit_ = firm->assetsB() / price_;

    if (same_ == 0) // First time, no history to give us an informed decision
        curr_up_ = true;
    else if (curr_profit_ > prev_profit_) // The last changed increased profit; keep going
        curr_up_ = prev_up_;
    else if (curr_profit_ < prev_profit_) // The last change decreased profit; turn around
        curr_up_ = !prev_up_;
    else // The last change did not change profit at all; lower price (this might help with some
         // perfect Bertrand cases)
        curr_up_ = false;
}

void PriceStepper::apply() {
    change_price();

    prev_up_ = curr_up_;
    prev_profit_ = curr_profit_;
}

void PriceStepper::change_price() {
    SharedMember<PriceFirm> firm = simulation()->agent(firm_);

    if (same_ == 0) {
        same_ = 1;
    }
    else {
        // Skip this stuff on the first run
        if (curr_up_ == prev_up_)
            ++same_;
        else // Changed direction, so reset the same direction counter
            same_ = 1;

        check_step();
    }

    // Change the price to 1+s or 1-s times the current price
    firm->setPrice((curr_up_ ? (1 + step_) : 1.0 / (1 + step_)) * firm->price());
}

void PriceStepper::check_step() {
    if (curr_up_ != prev_up_) {
        step_ /= 2;
        if (step_ < min_step) step_ = min_step;
    }
    else if (same_ > increase_) {
        step_ *= 2;

        same_ /= 2;
    }
}

} }
