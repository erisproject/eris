// Bundle methods.  Note that many of the small, more commonly-used methods are inlined (and thus
// declared in Bundle.hpp).

#include <eris/Bundle.hpp>
#include <unordered_set>
#include <set>

namespace eris {

BundleNegative::BundleNegative(const init_list &init) {
    for (auto g : init) set(g.first, g.second);
}
Bundle::Bundle(const init_list &init) {
    for (auto g : init) set(g.first, g.second);
}
void BundleNegative::clearZeros() {
    for (auto it = goods_.begin(); it != goods_.end(); ) {
        if (it->second == 0)
            it = goods_.erase(it);
        else
            ++it;
    }
}

int BundleNegative::erase(const eris_id_t &gid) {
    return goods_.erase(gid);
}

double BundleNegative::remove(const eris_id_t &gid) {
    double d = operator[](gid);
    erase(gid);
    return d;
}

bool Bundle::covers(const Bundle &b) const noexcept {
    for (auto g : b)
        if (g.second > 0 and (*this)[g.first] <= 0) return false;
    return true;
}
double Bundle::operator / (const Bundle &b) const noexcept {
    double mult = 0;
    for (auto g : goods_) {
        if (g.second > 0) {
            double theirs = b[g.first];
            if (theirs == 0) return std::numeric_limits<double>::infinity();
            double m = g.second / theirs;
            if (m > mult) mult = m;
        }
    }
    if (mult == 0 and b == 0) // Both bundles are zero bundles
        return std::numeric_limits<double>::quiet_NaN();

    return mult;
}
Bundle Bundle::operator % (const Bundle &b) const {
    Bundle ret = (*this / b) * b;
    ret -= *this;
    ret.clearZeros();
    return ret;
}

Bundle Bundle::common(const BundleNegative &a, const BundleNegative &b) noexcept {
    Bundle result;
    for (auto ag : a) {
        if (ag.second >= 0 and b.count(ag.first)) {
            double bq = b[ag.first];
            if (bq >= 0) result.set(ag.first, std::min<double>(ag.second, bq));
        }
    }
    return result;
}

Bundle Bundle::reduce(BundleNegative &a, BundleNegative &b) {
    if (&a == &b) throw std::invalid_argument("Bundle::reduce(a, b) called with &a == &b; a and b must be distinct objects");
    Bundle result = common(a, b);
    a -= result;
    b -= result;
    return result;
}

// All of the ==/</<=/>/>= methods are exactly the same, aside from the operator; this macro handles
// that.  REVOP is the reverse order version of the operator, needed for the static (e.g. 3 >= b)
// operator, as it just translate this into (b <= 3)
#define _ERIS_BUNDLE_CPP_COMPARE(OP, REVOP) \
bool BundleNegative::operator OP (const BundleNegative &b) const noexcept {\
    std::unordered_set<eris_id_t> goods;\
    for (auto g : goods_) goods.insert(goods.end(), g.first);\
    for (auto g : b.goods_) goods.insert(g.first);\
\
    for (auto g : goods)\
        if (!((*this)[g] OP b[g])) return false;\
    return true;\
}\
bool BundleNegative::operator OP (const double &q) const noexcept {\
    for (auto g : goods_)\
        if (!(g.second OP q)) return false;\
    return true;\
}\
bool operator OP (const double &q, const BundleNegative &b) noexcept {\
    return b REVOP q;\
}

_ERIS_BUNDLE_CPP_COMPARE(==, ==)
_ERIS_BUNDLE_CPP_COMPARE(<, >)
_ERIS_BUNDLE_CPP_COMPARE(<=, >=)
_ERIS_BUNDLE_CPP_COMPARE(>, <)
_ERIS_BUNDLE_CPP_COMPARE(>=, <=)

#undef _ERIS_BUNDLE_CPP_COMPARE


bool BundleNegative::operator != (const BundleNegative &b) const noexcept {
    return !(*this == b);
}
bool BundleNegative::operator != (const double &q) const noexcept {
    return !(*this == q);
}
bool operator != (const double &q, const BundleNegative &b) noexcept {
    return b != q;
}

// Prints everything *after* the "Bundle" or "BundleNegative" tag, i.e. starting from "(".
void BundleNegative::_print(std::ostream &os) const {
    os << "(";

    // Sort the keys:
    std::set<eris_id_t> keys;
    for (auto g : goods_)
        keys.insert(g.first);

    bool first = true;
    for (auto k : keys) {
        if (!first) os << ", ";
        else first = false;

        os << "[" << k << "]=" << goods_.at(k);
    }

    os << ")";
}

std::ostream& operator << (std::ostream &os, const BundleNegative &b) {
    os << ((dynamic_cast<const Bundle*>(&b) != NULL) ? "Bundle" : "BundleNegative");
    b._print(os);
    return os;
}
std::ostream& operator << (std::ostream &os, const Bundle &b) {
    os << "Bundle";
    b._print(os);
    return os;
}

}
