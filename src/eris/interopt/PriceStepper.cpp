#include <eris/interopt/PriceStepper.hpp>

namespace eris { namespace interopt {

PriceStepper::PriceStepper(const PriceFirm &firm, double step, int increase_count)
    : ProfitStepper(firm, step, increase_count) {}

void PriceStepper::take_step(double relative) {
    auto firm = simAgent<PriceFirm>(firm_);

    firm->setPrice(relative * firm->price());
}

} }
