#include <eris/firm/PriceFirm.hpp>
#include <algorithm>

namespace eris { namespace firm {

PriceFirm::PriceFirm(Bundle output, Bundle price, double capacity) :
    price_(price), output_(output), capacity_(capacity) {}


void PriceFirm::setPrice(Bundle price) noexcept {
    price_ = price;
}
const Bundle& PriceFirm::price() const noexcept {
    return price_;
}
void PriceFirm::setOutput(Bundle output) noexcept {
    output_ = output;
}
const Bundle& PriceFirm::output() const noexcept {
    return output_;
}

double PriceFirm::canSupplyAny(const Bundle &b) const noexcept {
    double s = canProduceAny(b);
    if (s >= std::numeric_limits<double>::infinity())
        return s;

    // Otherwise get the supply maximum by adding assets on hand to the maximum production, then
    // dividing into the requested bundle
    return (s*b + assets()) / b;
}

double PriceFirm::canProduceAny(const Bundle &b) const noexcept {
    if (!output_.covers(b) || capacity_used_ >= capacity_) return 0.0;

    // Return the maximum that we can produce, divided by the desired bundle.  Note that this could
    // well be infinity!
    return ((capacity_ - capacity_used_) * output_) / b;
}

void PriceFirm::produce(const Bundle &b) {
    if (!output_.covers(b))
        throw supply_mismatch();
    else if (capacity_used_ >= capacity_)
        throw production_constraint();

    double produce = b / output_;

    if (capacity_used_ + produce > capacity_) {
        // We can't produce enough, so error out
        throw production_constraint();
    }
    else {
        // Produce the needed amount
        capacity_used_ += produce;

        // Record any new surplus as a result of production:
        assets() += b % output_;
    }
}

double PriceFirm::produceAny(const Bundle &b) {
    if (!output_.covers(b))
        throw supply_mismatch();
    else if (capacity_used_ >= capacity_)
        return 0.0;

    // We want to produce this much in total:
    double want = b / output_;
    double produce = want;

    bool constrained = capacity_used_ + produce > capacity_;
    if (constrained) {
        // We can't produced the full amount desired; produce as much as possible
        // (the max is just here as a sanity check, to avoid numerical imprecision errors)
        produce = std::max<double>(capacity_ - capacity_used_, 0.0);
        capacity_used_ = capacity_;
    }
    else capacity_used_ += produce;

    // Production might have resulted in excess of some goods (for example, when we want (4,1)
    // but produce in ratios of (1,1), we'll have an excess of (0,3)), so skim off any excess
    // that may have resulted and add it to the surplus assets
    assets() += (produce * output_) % b;

    return constrained ? produce / want : 1.0;
}

void PriceFirm::advance() {
    Firm::advance();
    capacity_used_ = 0;
}

} }
