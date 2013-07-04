#include <eris/interopt/QFStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

QFStepper::QFStepper(const QFirm &qf, double step, int increase_count) :
    Stepper(step, increase_count), firm_(qf) {}

bool QFStepper::should_increase() const {
    SharedMember<QFirm> firm = simulation()->agent(firm_);

    // If we can't still supply any part of the output bundle, we should increase
    return !firm->supplies(firm->output());
}

void QFStepper::take_step(double relative) {
    SharedMember<QFirm> firm = simulation()->agent(firm_);

    firm->capacity *= relative;
}

void QFStepper::added() {
    dependsOn(firm_);
}

} }
