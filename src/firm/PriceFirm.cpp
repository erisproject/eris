#include "firm/PriceFirm.hpp"

namespace eris { namespace firm {

PriceFirm::PriceFirm(Bundle output, Bundle price, double capacity) :
    _price(price), _output(output), capacity(capacity) {}

bool PriceFirm::canProduce(const Bundle &b) const noexcept {
    return canProduceAny(b) == 1.0;
}


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

double PriceFirm::canProduceAny(const Bundle &b) const noexcept {
    if (!_output.covers(b) || capacityUsed >= capacity) return 0.0;
    double want = b / _output;
    return (capacityUsed + want <= capacity)
        ? 1.0
        : want / (capacity - capacityUsed);
}

bool PriceFirm::produce(const Bundle &b) {
    if (!_output.covers(b) || capacityUsed >= capacity) return false;
    double want = b / _output;
    if (capacityUsed + want <= capacity) {
        capacityUsed += want;
        return true;
    }
    return false;
}

double PriceFirm::produceAny(const Bundle &b) {
    if (!_output.covers(b) || capacityUsed >= capacity) return false;
    double want = b / _output;
    if (capacityUsed + want > capacity) {
        capacityUsed = capacity;
        return want / (capacity - capacityUsed);
    }

    capacityUsed += want;
    return 1.0;
}

void PriceFirm::advance() {
    capacityUsed = 0;
}

} }
