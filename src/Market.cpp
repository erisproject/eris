#include "Market.hpp"

namespace eris {

Market::Market(Bundle output, Bundle price) : _output(output), _price(price) {}

void Market::addFirm(SharedMember<Firm> f) {
    suppliers.insert(std::make_pair(f->id(), f));
}

void Market::removeFirm(eris_id_t fid) {
    suppliers.erase(fid);
}

}
