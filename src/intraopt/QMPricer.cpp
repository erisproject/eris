#include <eris/intraopt/QMPricer.hpp>
#include <eris/market/QMarket.hpp>

namespace eris { namespace intraopt {

QMPricer::QMPricer(const QMarket &qm, int tries, double initial_step, int increase_count)
    : market_id_(qm), stepper_(Stepper(initial_step, increase_count)), tries_(tries) {}

void QMPricer::initialize() {
    tried_ = 0;
}

void QMPricer::apply() {}

bool QMPricer::postOptimize() {
    bool first_try = tried_ == 0;

    // If we're all out of adjustments, don't change the price
    if (++tried_ > tries_) return false;

    auto qmarket = simMarket<QMarket>(market_id_);
    double excess_capacity = qmarket->firmQuantities();

    bool last_was_decrease = not stepper_.prev_up;
    bool increase_price = excess_capacity <= 0;

    if (not first_try and last_was_decrease and not increase_price and excess_capacity >= last_excess_) {
        // Decreasing the price last time didn't help anything--perhaps noise from other market
        // adjustments, but it could also mean that we've hit market satiation, in which case
        // decreasing price further won't help.
        increase_price = true;
    }
    last_excess_ = excess_capacity;

    double new_price = stepper_.step(increase_price);

    if (new_price != 1) {
        qmarket->setPrice(new_price * qmarket->price());
        return true;
    }
    return false;
}

} }

// vim:tw=100
