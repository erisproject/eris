#include <eris/firm/QFirm.hpp>

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

void QFirm::advance() {
    Bundle leftover = depreciate();
    FirmNoProd::advance(); // Clears assets
    assets() += leftover;
    ensureNext(capacity * output());
    updateStarted();
}

void QFirm::added() {
    ensureNext(capacity * output());
    updateStarted();
}

void QFirm::produceNext(const Bundle &b) {
    Bundle need = b - Bundle::common(b, assets());
    assets() += need.coverage(output()) * output();
}

void QFirm::updateStarted() {
    started = assets().multiples(output());
}

} }
