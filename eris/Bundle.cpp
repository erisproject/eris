// Bundle methods.  Note that many of the small, more commonly-used methods are inlined (and thus
// declared in Bundle.hpp).

#include <eris/Bundle.hpp>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>
#include <cmath>

namespace eris {

BundleSigned::BundleSigned() {}
BundleSigned::BundleSigned(MemberID g, double q) { set(g, q); }
BundleSigned::BundleSigned(const BundleSigned &b) {
    for (auto &g : b) set(g.first, g.second);
}
BundleSigned::BundleSigned(const std::initializer_list<std::pair<id_t, double>> &init) {
    for (auto &g : init) set(g.first, g.second);
}

Bundle::Bundle() : BundleSigned() {}
Bundle::Bundle(MemberID g, double q) : BundleSigned() { set(g, q); }
Bundle::Bundle(const BundleSigned &b) : BundleSigned() {
    for (auto &g : b) set(g.first, g.second);
}
Bundle::Bundle(const Bundle &b) : Bundle((BundleSigned&) b) {}
Bundle::Bundle(const std::initializer_list<std::pair<id_t, double>> &init) {
    for (auto &g : init) set(g.first, g.second);
}

constexpr double BundleSigned::zero_;
constexpr double BundleSigned::default_transfer_epsilon;

const double& BundleSigned::operator[] (MemberID gid) const {
    // Don't want to invoke map's [] operator, because it auto-vivifies the element
    auto &f = q_stack_.front();
    auto it = f.find(gid);
    return it == f.end() ? zero_ : it->second;
}
BundleSigned::valueproxy BundleSigned::operator[] (MemberID gid) {
    return valueproxy(*this, gid);
}

void BundleSigned::set(MemberID gid, double quantity) {
    q_stack_.front()[gid] = quantity;
}

void Bundle::set(MemberID gid, double quantity) {
    if (quantity < 0) throw negativity_error(gid, quantity);
    BundleSigned::set(gid, quantity);
}

bool BundleSigned::empty() const {
    return q_stack_.front().empty();
}
std::unordered_map<id_t, double>::size_type BundleSigned::size() const {
    return q_stack_.front().size();
}
int BundleSigned::count(MemberID gid) const {
    return q_stack_.front().count(gid);
}
std::unordered_map<id_t, double>::const_iterator BundleSigned::begin() const {
    return q_stack_.front().cbegin();
}
std::unordered_map<id_t, double>::const_iterator BundleSigned::end() const {
    return q_stack_.front().cend();
}

void BundleSigned::clearZeros() {
    auto &goods = q_stack_.front();
    for (auto it = goods.begin(); it != goods.end(); ) {
        if (it->second == 0)
            it = goods.erase(it);
        else
            ++it;
    }
}

void BundleSigned::clear() {
    q_stack_.front().clear();
}

int BundleSigned::erase(MemberID gid) {
    return q_stack_.front().erase(gid);
}

double BundleSigned::remove(MemberID gid) {
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

Bundle Bundle::common(const BundleSigned &a, const BundleSigned &b) noexcept {
    Bundle result;
    for (auto &ag : a) {
        if (ag.second >= 0 and b.count(ag.first)) {
            double bq = b[ag.first];
            if (bq >= 0) result.set(ag.first, std::min<double>(ag.second, bq));
        }
    }
    return result;
}

Bundle Bundle::reduce(BundleSigned &a, BundleSigned &b) {
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

Bundle BundleSigned::positive() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second > 0) b.set(g.first, g.second); }
    return b;
}

Bundle BundleSigned::negative() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second < 0) b.set(g.first, -g.second); }
    return b;
}

Bundle BundleSigned::zeros() const noexcept {
    Bundle b;
    for (auto &g : *this) { if (g.second == 0) b.set(g.first, 0); }
    return b;
}

Bundle& Bundle::operator *= (double m) {
    if (m < 0) throw negativity_error("Attempt to scale Bundle by negative value " + std::to_string(m), 0, m);
    BundleSigned::operator*=(m);
    return *this;
}
BundleSigned& BundleSigned::operator /= (double d) {
    return *this *= (1.0 / d);
}
Bundle& Bundle::operator /= (double d) {
    if (d < 0) throw negativity_error("Attempt to scale Bundle by negative value 1/" + std::to_string(d), 0, d);
    BundleSigned::operator/=(d);
    return *this;
}
BundleSigned BundleSigned::operator - () const {
    return *this * -1.0;
}
// Doxygen bug: doxygen erroneously warns about non-inlined friend methods
/// \cond
BundleSigned operator * (double m, const BundleSigned &b) {
    return b * m;
}
/// \endcond
Bundle operator * (double m, const Bundle &b) {
    return b * m;
}
BundleSigned BundleSigned::operator / (double d) const {
    return *this * (1.0/d);
}
Bundle Bundle::operator / (double d) const {
    return *this * (1.0/d);
}

// All of the overloaded ==/</<=/>/>= methods are exactly the same, aside from the algebraic
// operator; this macro handles that.  REVOP is the reverse order version of the operator, needed
// for the static (e.g. 3 >= b) operator, as it just translate this into (b <= 3)
#define _ERIS_BUNDLE_CPP_COMPARE(OP, REVOP) \
bool BundleSigned::operator OP (const BundleSigned &b) const noexcept {\
    std::unordered_set<id_t> goods;\
    for (auto &g : *this) goods.insert(g.first);\
    for (auto &g : b) goods.insert(g.first);\
\
    for (auto &g : goods)\
        if (!(operator[](g) OP b[g])) return false;\
    return true;\
}\
bool BundleSigned::operator OP (double q) const noexcept {\
    for (auto &g : *this)\
        if (!(g.second OP q)) return false;\
    return true;\
}\
bool operator OP (double q, const BundleSigned &b) noexcept {\
    return b REVOP q;\
}

_ERIS_BUNDLE_CPP_COMPARE(==, ==)
_ERIS_BUNDLE_CPP_COMPARE(<, >)
_ERIS_BUNDLE_CPP_COMPARE(<=, >=)
_ERIS_BUNDLE_CPP_COMPARE(>, <)
_ERIS_BUNDLE_CPP_COMPARE(>=, <=)

#undef _ERIS_BUNDLE_CPP_COMPARE


bool BundleSigned::operator != (const BundleSigned &b) const noexcept {
    return !(*this == b);
}
bool BundleSigned::operator != (double q) const noexcept {
    return !(*this == q);
}
bool operator != (double q, const BundleSigned &b) noexcept {
    return !(b == q);
}

BundleSigned& BundleSigned::operator = (const BundleSigned &b) {
    if (this == &b) return *this; // Assigning something to itself is a no-op.
    beginTransaction();
    try {
        clear();
        for (auto &g : b) set(g.first, g.second);
    }
    catch (...) { abortTransaction(); throw; }
    commitTransaction();
    return *this;
}

Bundle& Bundle::operator = (const Bundle &b) {
    BundleSigned::operator=(b);
    return *this;
}

#define _ERIS_BUNDLE_CPP_ADDSUB(OP, OPEQ)\
BundleSigned& BundleSigned::operator OPEQ (const BundleSigned &b) {\
    beginTransaction();\
    try { for (auto &g : b) set(g.first, operator[](g.first) OP g.second); }\
    catch (...) { abortTransaction(); throw; }\
    commitTransaction();\
    return *this;\
}\
Bundle& Bundle::operator OPEQ (const BundleSigned &b) {\
    BundleSigned::operator OPEQ(b);\
    return *this;\
}\
BundleSigned BundleSigned::operator OP (const BundleSigned &b) const {\
    BundleSigned ret(*this);\
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

BundleSigned& BundleSigned::operator *= (double m) {
    beginTransaction();
    try { for (auto &g : *this) set(g.first, g.second * m); }
    catch (...) { abortTransaction(); throw; }
    commitTransaction();
    return *this;
}

BundleSigned BundleSigned::operator * (double m) const {
    BundleSigned ret(*this);
    ret.beginEncompassing();
    ret *= m;
    ret.endEncompassing();
    return ret;
}

Bundle Bundle::operator * (double m) const {
    Bundle ret(*this);
    ret.beginEncompassing();
    ret *= m;
    ret.endEncompassing();
    return ret;
}

void BundleSigned::beginTransaction(const bool encompassing) noexcept {
    if (not encompassed_.empty()) {
        encompassed_.push_front(true);
        return;
    }

    // Duplicate the most recent Bundle, make it the new front of the stack.
    q_stack_.push_front(q_stack_.front());

    if (encompassing) encompassed_.push_front(true);
}

void BundleSigned::commitTransaction() {
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

void BundleSigned::abortTransaction() {
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

void BundleSigned::beginEncompassing() noexcept {
    encompassed_.push_front(false);
}

void BundleSigned::endEncompassing() {
    if (encompassed_.empty())
        throw no_transaction_exception("endEncompassing() called with no encompassing in effect");

    if (encompassed_.front())
        throw no_transaction_exception("endEncompassing() called to terminate beginTransaction()");

    encompassed_.pop_front();
}


BundleSigned BundleSigned::transfer(const BundleSigned &amount, BundleSigned &to, double epsilon) {
    beginTransaction(true);
    to.beginTransaction(true);
    BundleSigned actual;
    try {
        for (auto &g : amount) {
            double abs_transfer = std::abs(g.second);
            if (abs_transfer == 0) continue;
            bool transfer_to = g.second > 0;

            auto &src  = transfer_to ? *this : to;
            auto &dest = transfer_to ? to : *this;
            double q_src  = src[g.first];
            double q_dest = dest[g.first];

            if (std::abs(q_src - abs_transfer) < std::abs(epsilon*q_src))
                abs_transfer = q_src;
            else if (q_dest < 0 && std::abs(q_dest + abs_transfer) < std::abs(epsilon*q_dest))
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

BundleSigned BundleSigned::transfer(const BundleSigned &amount, double epsilon) {
    beginTransaction(true);
    BundleSigned actual;
    actual.beginEncompassing();
    try {
        for (auto &g : amount) {
            double abs_transfer = std::abs(g.second);
            if (abs_transfer == 0) continue;
            bool transfer_to = g.second > 0;

            double q = (*this)[g.first];
            if (transfer_to and std::abs(q - abs_transfer) < std::abs(epsilon * q))
                abs_transfer = q;
            else if (not transfer_to and q < 0 and std::abs(q + abs_transfer) < std::abs(epsilon * q))
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

BundleSigned BundleSigned::transferTo(BundleSigned &to, double epsilon) {
    beginTransaction(true);
    to.beginTransaction(true);
    BundleSigned ret = transfer(*this, to, epsilon);
    clear();
    commitTransaction();
    to.commitTransaction();
    return ret;
}


bool Bundle::hasApprox(const BundleSigned &amount, const Bundle &to, double epsilon) const {
    for (auto &g : amount) {
        double abs_transfer = std::abs(g.second);
        if (abs_transfer == 0) continue;

        const double &q = g.second > 0 ? (*this)[g.first] : to[g.first];

        // If the final amount is lower than -ε*(original q), the transfer will fail
        if (q - abs_transfer <= -epsilon * q)
            return false;
    }
    return true;
}

bool Bundle::hasApprox(const BundleSigned &amount, double epsilon) const {
    for (auto &g : amount) {
        if (g.second <= 0) continue;

        const double &q = (*this)[g.first];

        // If the final amount is lower than -ε*(original q), the transfer will fail
        if (q - g.second <= -epsilon * q)
            return false;
    }
    return true;
}

// Prints everything *after* the "Bundle" or "BundleSigned" tag, i.e. starting from "(".
void BundleSigned::_print(std::ostream &os) const {
    os << "(";

    // Sort the keys:
    std::vector<id_t> keys;
    keys.reserve(size());
    for (auto &g : *this)
        keys.push_back(g.first);
    std::sort(keys.begin(), keys.end());

    bool first = true;
    for (auto k : keys) {
        if (!first) os << ", ";
        else first = false;

        os << "[" << k << "]=" << operator[](k);
    }

    os << ")";
}

std::ostream& operator << (std::ostream &os, const BundleSigned &b) {
    os << ((dynamic_cast<const Bundle*>(&b) != NULL) ? "Bundle" : "BundleSigned");
    b._print(os);
    return os;
}
std::ostream& operator << (std::ostream &os, const Bundle &b) {
    os << "Bundle";
    b._print(os);
    return os;
}

}
