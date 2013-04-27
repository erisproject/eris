#include <Bundle.hpp>

double Bundle::operator[] (eris_id_t gid) {
    return bundle.count(gid) ? bundle.at(gid) : 0.0;
}

void Bundle::set (eris_id_t gid, double quantity) {
    if (quantity < 0)
        throw 123; // FIXME
    else if (quantity == 0.0)
        bundle.erase(gid);
    else bundle[gid] = quantity;
}
void Bundle::Negative::set (eris_id_t gid, double quantity) {
    if (quantity < 0)
        bundle[gid] = quantity;
    else
        Bundle::set(gid, quantity);
}

std::map<eris_id_t, double>::const_iterator Bundle::begin() {
    return bundle.cbegin();
}
std::map<eris_id_t, double>::const_iterator Bundle::end() {
    return bundle.cend();
}
