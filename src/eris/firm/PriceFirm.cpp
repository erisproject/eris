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
    return (s*b + assets()).multiples(b);
}

double PriceFirm::canProduceAny(const Bundle &b) const noexcept {
    if (!output_.covers(b) || capacity_used_ >= capacity_) return 0.0;

    // Return the maximum that we can produce, divided by the desired bundle.  Note that this could
    // well be infinity!
    return ((capacity_ - capacity_used_) * output_).multiples(b);
}

Bundle PriceFirm::produce(const Bundle &b) {
    if (!output_.covers(b))
        throw supply_mismatch();

    // Produce the smallest multiple of output_ required to cover b.  e.g. if b is (3,3) and output_
    // is (1,3), we produce (3,9).
    return b.coverage(output_) * output_;
}

/*
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
}*/

void PriceFirm::reserveProduction(const Bundle &reserve) {
    if (!output_.covers(reserve))
        throw supply_mismatch();

    double need = reserve.coverage(output_);
    if (need + capacity_used_ > capacity_)
        throw production_constraint();

    Bundle to_produce = need * output_;
    Bundle extra = to_produce - reserve;

    capacity_used_ += need;
    reserved_production_ += reserve;
    if (extra != 0)
        excess_production_ += extra;
}

void PriceFirm::reduceExcessProduction() {
    double excess = excess_production_.multiples(output_);
    if (excess > 0) {
        capacity_used_ -= excess;
        excess_production_ -= excess*output_;
    }
}

void PriceFirm::interAdvance() {
    capacity_used_ = 0;
}

} }
