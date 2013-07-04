#include <eris/firm/PriceFirm.hpp>
#include <algorithm>

namespace eris { namespace firm {

PriceFirm::PriceFirm(Bundle output, Bundle price, double capacity) :
    _price(price), _output(output), capacity(capacity) {}


void PriceFirm::setPrice(Bundle price) noexcept {
    _price = price;
}
const Bundle& PriceFirm::price() const noexcept {
    return _price;
}
void PriceFirm::setOutput(Bundle output) noexcept {
    _output = output;
}
const Bundle& PriceFirm::output() const noexcept {
    return _output;
}

double PriceFirm::canSupplyAny(const Bundle &b) const noexcept {
    double s = canProduceAny(b);
    if (s >= std::numeric_limits<double>::infinity())
        return s;

    // Otherwise get the supply maximum by adding assets on hand to the maximum production, then
    // dividing into the requested bundle
    return (s*b + assetsB()) / b;
}

double PriceFirm::canProduceAny(const Bundle &b) const noexcept {
    if (!_output.covers(b) || capacityUsed >= capacity) return 0.0;

    // Return the maximum that we can produce, divided by the desired bundle.  Note that this could
    // well be infinity!
    return ((capacity - capacityUsed) * _output) / b;
}

void PriceFirm::produce(const Bundle &b) {
    if (!_output.covers(b))
        throw supply_mismatch();
    else if (capacityUsed >= capacity)
        throw production_constraint();

    double produce = b / _output;

    if (capacityUsed + produce > capacity) {
        // We can't produce enough, so error out
        throw production_constraint();
    }
    else {
        // Produce the needed amount
        capacityUsed += produce;

        // Record any new surplus as a result of production:
        assets() += b % _output;
    }
}

double PriceFirm::produceAny(const Bundle &b) {
    if (!_output.covers(b))
        throw supply_mismatch();
    else if (capacityUsed >= capacity)
        return 0.0;

    // We want to produce this much in total:
    double want = b / _output;
    double produce = want;

    bool constrained = capacityUsed + produce > capacity;
    if (constrained) {
        // We can't produced the full amount desired; produce as much as possible
        // (the max is just here as a sanity check, to avoid numerical imprecision errors)
        produce = std::max<double>(capacity - capacityUsed, 0.0);
        capacityUsed = capacity;
    }
    else capacityUsed += produce;

    // Production might have resulted in excess of some goods (for example, when we want (4,1)
    // but produce in ratios of (1,1), we'll have an excess of (0,3)), so skim off any excess
    // that may have resulted and add it to the surplus assets
    assets() += (produce * _output) % b;

    return constrained ? produce / want : 1.0;
}

void PriceFirm::advance() {
    Firm::advance();
    capacityUsed = 0;
}

} }
