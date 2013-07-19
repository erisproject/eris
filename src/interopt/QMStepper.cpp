#include <eris/interopt/QMStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

QMStepper::QMStepper(const Quantity &qm, double step, int increase_count) :
    Stepper(step, increase_count), market_(qm) {}

bool QMStepper::should_increase() const {
    auto market = simulation()->market<Quantity>(market_);

    return market->quantity_ > 0;
}

void QMStepper::take_step(double relative) {
    auto market = simulation()->market<Quantity>(market_);

    market->price_ *= relative;
}

void QMStepper::added() {
    dependsOn(market_);
}

} }
