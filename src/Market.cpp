#include <eris/Market.hpp>

namespace eris {

Market::Market(Bundle output_unit, Bundle price_unit) : output_unit(output_unit), price_unit(price_unit) {}

void Market::addFirm(SharedMember<Firm> f) {
    suppliers.insert(std::make_pair(f->id(), f));
}

void Market::removeFirm(eris_id_t fid) {
    suppliers.erase(fid);
}

}
