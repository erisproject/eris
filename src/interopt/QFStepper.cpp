#include <eris/interopt/QFStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace interopt {

QFStepper::QFStepper(const QFirm &qf, double step, int increase_count) :
    JumpStepper(step, increase_count, 1), firm_(qf) {}

bool QFStepper::should_increase() const {
    auto firm = simulation()->agent<QFirm>(firm_);

    // If we can't still supply any part of the output bundle, we should increase
    return !firm->supplies(firm->output());
}

void QFStepper::take_step(double relative) {
    auto firm = simulation()->agent<QFirm>(firm_);

    firm->capacity *= relative;
}

bool QFStepper::should_jump() const {
    auto firm = simulation()->agent<QFirm>(firm_);

    double sales = firm->started - firm->assets().multiples(firm->output());
    if (sales <= firm->capacity / 2) {
        jump_cap_ = firm->capacity / 2;
        return true;
    }
    return false;
}

void QFStepper::take_jump() {
    firm->capacity = jump_cap_;
}

void QFStepper::added() {
    dependsOn(firm_);
}

} }
