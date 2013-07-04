#include <eris/firm/QFirm.hpp>

namespace eris { namespace firm {

QFirm::QFirm(Bundle out, double initial_capacity, double depr) : capacity(initial_capacity), output_unit_(out) {
    depreciation(depr);
}

void QFirm::depreciate() {
    Bundle &a = assets();
    for (auto g : output()) {
        a.set(g.first, a[g.first] * depreciation_);
    }
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
    depreciate();
    ensureNext(capacity * output());
}

void QFirm::added() {
    ensureNext(capacity * output());
}

void QFirm::produceNext(const Bundle &b) {
    Bundle need = b - Bundle::common(b, assets());
    assets() += (need / output()) * output();
}

} }
