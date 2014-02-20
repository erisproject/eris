#include <eris/firm/QFirm.hpp>
#include <cmath>

namespace eris { namespace firm {

QFirm::QFirm(Bundle out, double initial_capacity, double depr) : capacity(initial_capacity), output_unit_(out) {
    depreciation(depr);
}

Bundle QFirm::depreciate() const {
    const Bundle &a = assets();
    Bundle remaining;
    double left_mult = 1 - depreciation();
    if (left_mult > 0) {
        for (auto g : output()) {
            remaining.set(g.first, a[g.first] * left_mult);
        }
    }
    return remaining;
}

double QFirm::depreciation() const noexcept {
    return depreciation_;
}

void QFirm::depreciation(const double &depr) noexcept {
    depreciation_ = depr < 0 ? 0 : depr > 1 ? 1 : depr;
}

const Bundle& QFirm::output() const noexcept {
    return output_unit_;
}

void QFirm::interAdvance() {
    // Clear everything except what is left by depreciation
    assets() = depreciate();
}

void QFirm::intraInitialize() {
    ensureNext(capacity * output());
    updateStarted();
}

void QFirm::produceNext(const Bundle &b) {
    double quantity = b.coverage(output());
    if (not std::isfinite(quantity))
        throw supply_mismatch();
    assets() += quantity * output();
}

void QFirm::updateStarted() {
    started = assets().multiples(output());
}

} }
