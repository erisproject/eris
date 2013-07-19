#include <eris/interopt/QFStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

QFStepper::QFStepper(const QFirm &qf, double step, int increase_count) :
    Stepper(step, increase_count), firm_(qf) {}

bool QFStepper::should_increase() const {
    auto firm = simulation()->agent<QFirm>(firm_);

    // If we can't still supply any part of the output bundle, we should increase
    return !firm->supplies(firm->output());
}

void QFStepper::take_step(double relative) {
    auto firm = simulation()->agent<QFirm>(firm_);

    firm->capacity *= relative;
}

void QFStepper::added() {
    dependsOn(firm_);
}

} }
