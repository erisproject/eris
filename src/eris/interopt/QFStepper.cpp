#include <eris/interopt/QFStepper.hpp>

namespace eris { namespace interopt {

QFStepper::QFStepper(const QFirm &qf, const Bundle &profit_basis, double step, int increase_count) :
    ProfitStepper(qf, profit_basis, step, increase_count, 1), firm_(qf) {}

void QFStepper::take_step(double relative) {
    auto firm = simAgent<QFirm>(firm_);

    firm->capacity *= relative;
}

bool QFStepper::should_jump() const {
    auto firm = simAgent<QFirm>(firm_);

    double sales = firm->started - firm->assets().multiples(firm->output());
    if (sales <= firm->capacity / 2) {
        jump_cap_ = sales;
        return true;
    }
    return false;
}

void QFStepper::take_jump() {
auto firm = simAgent<QFirm>(firm_);
    firm->capacity = jump_cap_;
}

} }
