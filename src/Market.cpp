#include "Market.hpp"

namespace eris {

Market::Market(Bundle output, Bundle priceUnit) {
    setOutput(output);
    setPriceUnit(priceUnit);
}

const Bundle Market::priceBundle(double q) const { return _price * price(q); }

const Bundle Market::buyBundle(double q, double p, const Bundle &assets) { return _price * buy(q, p, assets); }

void Market::setOutput(Bundle out)          { _output = std::move(out); }
void Market::setPriceUnit(Bundle priceUnit) { _price = std::move(priceUnit); }

const Bundle Market::output()    const { return _output; }
const Bundle Market::priceUnit() const { return _price; }

void Market::addFirm(SharedMember<Firm> f) {
    suppliers.insert(std::make_pair(f->id(), f));
}

void Market::removeFirm(eris_id_t fid) {
    suppliers.erase(fid);
}

}
