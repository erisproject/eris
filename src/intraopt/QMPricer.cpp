#include <eris/intraopt/QMPricer.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace intraopt {

QMPricer::QMPricer(const Quantity &qm, int tries, double initial_step, int increase_count)
    : market_id_(qm), stepper_(Stepper(initial_step, increase_count)), tries_(tries) {}

void QMPricer::initialize() {
    tried_ = 0;
}

void QMPricer::apply() {}

bool QMPricer::postOptimize() {
    // We're all out of adjustments, so don't change the price
    if (++tried_ > tries_) return false;

    auto qmarket = simMarket<Quantity>(market_id_);
    double agg_q = qmarket->firmQuantities();
    bool increase_price = agg_q <= 0;

    double new_price = stepper_.step(increase_price);

    if (new_price != 1) {
        qmarket->setPrice(new_price * qmarket->price());
        return true;
    }
    return false;
}

} }
