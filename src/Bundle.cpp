#include <Bundle.hpp>

double BundleNegative::operator[] (eris_id_t gid) const {
    return bundle.count(gid) ? bundle.at(gid) : 0.0;
}

void BundleNegative::set (eris_id_t gid, double quantity) {
    if (quantity == 0)
        bundle.erase(gid);
    else
        bundle[gid] = quantity;
}

void Bundle::set (eris_id_t gid, double quantity) {
    if (quantity < 0)
        throw "FIXME - quantity not >= 0!"; // FIXME

    BundleNegative::set(gid, quantity);
}

std::map<eris_id_t, double>::const_iterator BundleNegative::begin() {
    return bundle.cbegin();
}
std::map<eris_id_t, double>::const_iterator BundleNegative::end() {
    return bundle.cend();
}
