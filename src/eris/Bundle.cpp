// Bundle methods.  Note that many of the small, more commonly-used methods are inlined (and thus
// declared in Bundle.hpp).

#include <eris/Bundle.hpp>
#include <unordered_set>
#include <set>
#include <cmath>

#include <iostream>

namespace eris {

BundleNegative::BundleNegative(const std::initializer_list<std::pair<eris_id_t, double>> &init) {
    for (auto &g : init) set(g.first, g.second);
}
Bundle::Bundle(const std::initializer_list<std::pair<eris_id_t, double>> &init) {
    for (auto &g : init) set(g.first, g.second);
}
void BundleNegative::clearZeros() {
    auto &goods = q_stack_.front();
    for (auto it = goods.begin(); it != goods.end(); ) {
        if (it->second == 0)
            it = goods.erase(it);
        else
            ++it;
    }
}

void BundleNegative::clear() {
    q_stack_.front().clear();
}

int BundleNegative::erase(const eris_id_t &gid) {
    return q_stack_.front().erase(gid);
}

double BundleNegative::remove(const eris_id_t &gid) {
    double d = operator[](gid);
    erase(gid);
    return d;
}

bool Bundle::covers(const Bundle &b) const noexcept {
    for (auto &g : b)
        if (g.second > 0 and operator[](g.first) <= 0) return false;
    return true;
}
double Bundle::coverage(const Bundle &b) const noexcept {
    double mult = 0;
    for (auto &g : *this) {
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
    ret.beginEncompassing();
    ret -= *this;
    ret.clearZeros();
    ret.endEncompassing();
    return ret;
}

double Bundle::multiples(const Bundle &b) const noexcept {
    double mult = std::numeric_limits<double>::infinity();
    for (auto &g : b) {
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
    for (auto &ag : a) {
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
    a.beginTransaction(true);
    b.beginTransaction(true);
    try {
        a -= result;
        b -= result;
    }
    catch (...) {
        a.abortTransaction();
        b.abortTransaction();
        throw;
    }
    a.commitTransaction();
    b.commitTransaction();
    return result;
}

Bundle BundleNegative::positive() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second > 0) b.set(g.first, g.second); }
    return b;
}

Bundle BundleNegative::negative() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second < 0) b.set(g.first, -g.second); }
    return b;
}

Bundle BundleNegative::zeros() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second == 0) b.set(g.first, 0); }
    return b;
}

// All of the overloaded ==/</<=/>/>= methods are exactly the same, aside from the algebraic
// operator; this macro handles that.  REVOP is the reverse order version of the operator, needed
// for the static (e.g. 3 >= b) operator, as it just translate this into (b <= 3)
#define _ERIS_BUNDLE_CPP_COMPARE(OP, REVOP) \
bool BundleNegative::operator OP (const BundleNegative &b) const noexcept {\
    std::unordered_set<eris_id_t> goods;\
    for (auto &g : *this) goods.insert(g.first);\
    for (auto &g : b) goods.insert(g.first);\
\
    for (auto &g : goods)\
        if (!(operator[](g) OP b[g])) return false;\
    return true;\
}\
bool BundleNegative::operator OP (const double &q) const noexcept {\
    for (auto &g : *this)\
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
    return !(b == q);
}

BundleNegative& BundleNegative::operator = (const BundleNegative &b) {
    beginTransaction();
    try {
        clear();
        for (auto &g : b) set(g.first, g.second);
    }
    catch (...) { abortTransaction(); throw; }
    commitTransaction();
    return *this;
}

#define _ERIS_BUNDLE_CPP_ADDSUB(OP, OPEQ)\
BundleNegative& BundleNegative::operator OPEQ (const BundleNegative &b) {\
    beginTransaction();\
    try { for (auto &g : b) set(g.first, operator[](g.first) OP g.second); }\
    catch (...) { abortTransaction(); throw; }\
    commitTransaction();\
    return *this;\
}\
BundleNegative BundleNegative::operator OP (const BundleNegative &b) const {\
    BundleNegative ret(*this);\
    ret.beginEncompassing();\
    ret OPEQ b;\
    ret.endEncompassing();\
    return ret;\
}

_ERIS_BUNDLE_CPP_ADDSUB(+, +=)
_ERIS_BUNDLE_CPP_ADDSUB(-, -=)

#undef _ERIS_BUNDLE_CPP_ADDSUB

Bundle Bundle::operator + (const Bundle &b) const {
    Bundle ret(*this);
    ret.beginEncompassing();
    ret += b;
    ret.endEncompassing();
    return ret;
}
Bundle Bundle::operator - (const Bundle &b) const {
    Bundle ret(*this);
    ret.beginEncompassing();
    ret -= b;
    ret.endEncompassing();
    return ret;
}

BundleNegative& BundleNegative::operator *= (const double &m) {
    beginTransaction();
    try { for (auto &g : *this) set(g.first, g.second * m); }
    catch (...) { abortTransaction(); throw; }
    commitTransaction();
    return *this;
}

BundleNegative BundleNegative::operator * (const double &m) const {
    BundleNegative ret(*this);
    ret.beginEncompassing();
    ret *= m;
    ret.endEncompassing();
    return ret;
}

Bundle Bundle::operator * (const double &m) const {
    Bundle ret(*this);
    ret.beginEncompassing();
    ret *= m;
    ret.endEncompassing();
    return ret;
}

void BundleNegative::beginTransaction(const bool &encompassing) noexcept {
    if (not encompassed_.empty()) {
        encompassed_.push_front(true);
        return;
    }

    // Duplicate the most recent Bundle, make it the new front of the stack.
    q_stack_.push_front(q_stack_.front());

    if (encompassing) encompassed_.push_front(true);
}

void BundleNegative::commitTransaction() {
    if (not encompassed_.empty()) {
        if (not encompassed_.front())
            throw no_transaction_exception("commitTransaction() called to terminate beginEncompassing()");

        encompassed_.pop_front();
        if (not encompassed_.empty()) return; // Still encompassed
    }
    // If we get here, we're not (or no longer) encompassed

    // Make sure there is actually a transaction to commit
    if (++(q_stack_.cbegin()) == q_stack_.cend())
        throw no_transaction_exception("commitTransaction() called with no transaction in effect");

    // Remove the *second* element from the stack: the first one is taking it over.
    q_stack_.erase_after(q_stack_.begin());
}

void BundleNegative::abortTransaction() {
    if (not encompassed_.empty()) {
        if (not encompassed_.front())
            throw no_transaction_exception("abortTransaction() called to terminate beginEncompassing()");

        encompassed_.pop_front();
        if (not encompassed_.empty()) return; // Still encompassed
    }
    // If we get here, we're not (or not longer) encompassed

    // Make sure there is actually a transaction to abort
    if (++(q_stack_.cbegin()) == q_stack_.cend()) throw no_transaction_exception("abortTransaction() called with no transaction in effect");

    // Remove the first element from the stack: it's been aborted.
    q_stack_.pop_front();
}

void BundleNegative::beginEncompassing() noexcept {
    encompassed_.push_front(false);
}

void BundleNegative::endEncompassing() {
    if (encompassed_.empty())
        throw no_transaction_exception("endEncompassing() called with no encompassing in effect");

    if (encompassed_.front())
        throw no_transaction_exception("endEncompassing() called to terminate beginTransaction()");

    encompassed_.pop_front();
}


BundleNegative BundleNegative::transferApprox(BundleNegative amount, BundleNegative &to, double epsilon) {
    beginTransaction(true);
    to.beginTransaction(true);
    BundleNegative actual;
    try {
        for (auto &g : amount) {
            double abs_transfer = fabs(g.second);
            if (abs_transfer == 0) continue;
            bool transfer_to = g.second > 0;

            auto &src  = transfer_to ? *this : to;
            auto &dest = transfer_to ? to : *this;
            double q_src  = src[g.first];
            double q_dest = dest[g.first];

            if (fabs(q_src - abs_transfer) < fabs(epsilon*q_src))
                abs_transfer = q_src;
            else if (q_dest < 0 and fabs(q_dest + abs_transfer) < fabs(epsilon*q_dest))
                abs_transfer = -q_dest;

            if (transfer_to) {
                set(g.first, q_src - abs_transfer);
                to.set(g.first, q_dest + abs_transfer);
                actual.set(g.first, abs_transfer);
            }
            else {
                to.set(g.first, q_src - abs_transfer);
                set(g.first, q_dest + abs_transfer);
                actual.set(g.first, -abs_transfer);
            }
        }
        clearZeros();
        to.clearZeros();
        actual.clearZeros();
    }
    catch (...) {
        abortTransaction();
        to.abortTransaction();
        throw;
    }
    commitTransaction();
    to.commitTransaction();
    return actual;
}

BundleNegative BundleNegative::transferApprox(BundleNegative amount, double epsilon) {
    beginTransaction(true);
    BundleNegative actual;
    actual.beginEncompassing();
    try {
        for (auto &g : amount) {
            double abs_transfer = fabs(g.second);
            if (abs_transfer == 0) continue;
            bool transfer_to = g.second > 0;

            double q = (*this)[g.first];
            if (transfer_to and fabs(q - abs_transfer) < fabs(epsilon * q))
                abs_transfer = q;
            else if (not transfer_to and q < 0 and fabs(q + abs_transfer) < fabs(epsilon * q))
                abs_transfer = -q;

            if (transfer_to) {
                set(g.first, q - abs_transfer);
                actual.set(g.first, abs_transfer);
            }
            else {
                set(g.first, q + abs_transfer);
                actual.set(g.first, -abs_transfer);
            }
        }
        clearZeros();
    }
    catch (...) {
        abortTransaction();
        throw;
    }
    commitTransaction();
    actual.endEncompassing();
    return actual;
}

// Prints everything *after* the "Bundle" or "BundleNegative" tag, i.e. starting from "(".
void BundleNegative::_print(std::ostream &os) const {
    os << "(";

    // Sort the keys:
    std::set<eris_id_t> keys;
    for (auto &g : *this)
        keys.insert(g.first);

    bool first = true;
    for (auto &k : keys) {
        if (!first) os << ", ";
        else first = false;

        os << "[" << k << "]=" << operator[](k);
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
