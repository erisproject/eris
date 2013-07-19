#include <eris/intraopt/QMPricer.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace intraopt {

QMPricer::QMPricer(const Quantity &qm, int tries, double initial_step, int increase_count)
    : stepper_(Stepper(initial_step, increase_count)), tries_(tries) {}

void QMPricer::initialize() {
    tried_ = 0;
}

void QMPricer::apply() {}

bool QMPricer::postOptimize() {
    // We're all out of adjustments, so don't change the price
    if (++tried_ > tries_) return false;

    auto market = simulation()->market<Quantity>(firm_id_);
    bool increase_price = market->firmQuantities() <= 0;

    double new_price = stepper_.step(increase_price);

    if (new_price != 1) {
        market->setPrice(new_price * market->price());
        return true;
    }
    return false;
}

} }
