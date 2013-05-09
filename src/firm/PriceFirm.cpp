#include "firm/PriceFirm.hpp"
#include <algorithm>

namespace eris { namespace firm {

PriceFirm::PriceFirm(Bundle output, Bundle price, double capacity) :
    _price(price), _output(output), capacity(capacity) {}


void PriceFirm::setPrice(Bundle price) noexcept {
    _price = price;
}
const Bundle PriceFirm::price() const noexcept {
    return _price;
}
void PriceFirm::setOutput(Bundle output) noexcept {
    _output = output;
}
const Bundle PriceFirm::output() const noexcept {
    return _output;
}

double PriceFirm::canSupplyAny(const Bundle &b) const noexcept {
    if (!_output.covers(b) || capacityUsed >= capacity) return 0.0;

    // Return the stock + maximum we can produce, divided by the desired bundle.  Note that this
    // could well be infinite!
    return (stock + (capacity - capacityUsed) * _output) / b;
}

void PriceFirm::supply(const Bundle &b) {
    if (!_output.covers(b))
        throw supply_mismatch();
    else if (capacityUsed >= capacity)
        throw supply_constraint();

    // Check to see if we already have some excess product stored up
    Bundle onhand = Bundle::common(stock, b);
    Bundle bNeed = b - onhand;

    double produce = bNeed / _output;

    if (produce == 0) {
        // We have enough on-hand to supply without production
        stock -= onhand;
    }
    else if (capacityUsed + produce > capacity) {
        // We can't produce enough, so error out
        throw supply_constraint();
    }
    else {
        // Produce the needed amount
        capacityUsed += produce;
        // Keep track of any surplus we used up
        stock -= onhand;
        // Record any new surplus as a result of production:
        stock += bNeed % _output;
    }
}

double PriceFirm::supplyAny(const Bundle &b) {
    if (!_output.covers(b))
        throw supply_mismatch();
    else if (capacityUsed >= capacity)
        return 0.0;

    Bundle bNeed = b;

    // Pull as much as possible out of surplus stock first:
    Bundle onhand = Bundle::reduce(stock, bNeed);

    // We want to produce this much more:
    double produce = bNeed / _output;

    // Short-circuit if current surplus was enough to supply the entire request
    if (produce == 0) return 1.0;

    bool constrained = capacityUsed + produce > capacity;
    if (constrained) {
        // We can't produced the full amount desired; produce as much as possible
        // (the max is just here as a sanity check, to avoid numerical imprecision errors)
        produce = std::max<double>(capacity - capacityUsed, 0.0);
        capacityUsed = capacity;
    }
    else capacityUsed += produce;

    if (produce > 0) {
        // Produce the additional amount we need (or as much as is available, if constrained)
        onhand += produce * _output;
        // Production might have resulted in excess of some goods (for example, when we want (4,1)
        // but produce in ratios of (1,1), we'll have an excess of (0,3)), so skim off any excess
        // that may have resulted and put it back into the surplus stock
        Bundle excess = onhand % b;
        stock += excess;

        // NB: commented out because it's just wasted calculations: the division below implicitly
        // discounts excesses.  (if something else dealing with onhand gets added below this point,
        // this should be uncommented):
        //
        //onhand -= excess;
    }

    return constrained ? onhand / b : 1.0;
}

void PriceFirm::advance() {
    capacityUsed = 0;
}

} }
