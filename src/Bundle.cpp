// Bundle methods.  Note that many of the small, more commonly-used methods are inlined (and thus
// declared in Bundle.hpp).

#include <eris/Bundle.hpp>
#include <unordered_set>
#include <set>
#include <cmath>

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

void BundleNegative::clear() {
    for (auto it = goods_.begin(); it != goods_.end(); )
        it = goods_.erase(it);
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
double Bundle::coverage(const Bundle &b) const noexcept {
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
Bundle Bundle::coverageExcess(const Bundle &b) const {
    Bundle ret = coverage(b) * b;
    ret -= *this;
    ret.clearZeros();
    return ret;
}

double Bundle::multiples(const Bundle &b) const noexcept {
    double mult = std::numeric_limits<double>::infinity();
    for (auto g : b) {
        if (g.second > 0) {
            double mine = (*this)[g.first];
            if (mine == 0) return 0.0;
            double m = mine / g.second;
            if (m < mult) mult = m;
        }
    }

    if (mult == std::numeric_limits<double>::infinity() and *this == 0)
        // Both are 0 bundles
        return std::numeric_limits<double>::quiet_NaN();

    return mult;
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

Bundle BundleNegative::positive() const noexcept {
    Bundle b;
    for (auto g : *this) { if (g.second > 0) b.set(g.first, g.second); }
    return b;
}

Bundle BundleNegative::negative() const noexcept {
    Bundle b;
    for (auto g : *this) { if (g.second < 0) b.set(g.first, -g.second); }
    return b;
}

Bundle BundleNegative::zeros() const noexcept {
    Bundle b;
    for (auto g : *this) { if (g.second == 0) b.set(g.first, g.second); }
    return b;
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

void BundleNegative::transferApprox(const BundleNegative &amount, BundleNegative &to, double epsilon) {
    for (auto g : amount) {
        double abs_transfer = fabs(g.second);
        if (abs_transfer == 0) continue;
        bool transfer_to = g.second > 0;

        double q_src  = transfer_to ? operator[](g.first) : to[g.first];
        double q_dest = transfer_to ? to[g.first] : operator[](g.first);

        if (fabs(q_src - abs_transfer) < fabs(epsilon*q_src))
            abs_transfer = q_src;
        else if (q_dest < 0 and fabs(q_dest + abs_transfer) < fabs(epsilon*q_dest))
            abs_transfer = -q_dest;

        if (transfer_to) {
            set(g.first, q_src - abs_transfer);
            to.set(g.first, q_dest + abs_transfer);
        }
        else {
            to.set(g.first, q_src - abs_transfer);
            set(g.first, q_dest + abs_transfer);
        }
    }
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
