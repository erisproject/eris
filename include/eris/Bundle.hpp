#pragma once
#include <eris/types.hpp>
#include <algorithm>
#include <exception>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace eris {

// A Bundle is a thin wrapper around a std::unordered_map designed for storing
// a set of goods and associated quantities.
//
// BundleNegative is a slightly more general class (Bundle is actually a specialization of
// BundleNegative) that allows any double value in the quantity; Bundle allows only non-negative
// quantities.
//
// Accessing quantities is done through the [] operation (e.g. bundle[gid]).  Accessing a good that
// does not exist returns a value of 0.
//
// Setting values is done through the set(id, value) method, *not* using the [] operator (since
// assignment may require checking, e.g.  for positive quantities).
//
// You can iterate through goods via the usual begin()/end() pattern; note that
// these get mapped to through to the underlying
// std::unordered_map<eris_id_t,double>'s cbegin()/cend() methods, and so are
// immutable.
//
// size() returns the number of goods in the bundle.  Note that values that have not been explicitly
// set (and thus return a value of 0) are *not* included in the size(), but values that have been
// explitly set (even to 0) *are* included.
//
// empty() returns true if size() == 0, false otherwise.  Note that empty() is not true for a bundle
// with explicit quantities of 0; for testing whether a bundle is empty in the sense of all
// quantities being 0 (explicitly or implicitly by omission), use the a == 0 operator, discussed
// below.
//
// count(id) returns 1 if the id exists in the Bundle (even if it equals 0), 0 otherwise.
//
// erase(id) removes the good `id' from the bundle (if it exists), and returns either 0 or 1
// indicating whether the good was present in the bundle, like std::unordered_map::erase.
//
// remove(id) is like erase(id), except it returns the quantity of the removed good, or 0 if the
// good was not in the bundle.
//
// clearZeros() removes any goods from the bundle that have a quantity set to 0.
//
// The usual +, -, *, / operators as overloaded as expected for adding/scaling bundles, plus the
// analogous +=, -=, *=, and /= operators.  After addition or subtraction, the result will contain
// all goods that existed in either good.  Unary negative is defined, but always returns a
// BundleNegative (even for a Bundle).
//
// Comparison operators are also overloaded.  Each of ==, >, >=, <, and <= is true iff all the
// inequality relation is satisfied for the quantities of every good in either bundle.  Note that !=
// is also overloaded, but different from the above: a != b is equivalent to !(a == b).  Goods that
// are missing from one bundle or the other are implicitly assumed to have value 0.  Comparison can
// also be done against a constant in which case the quantity of each good must satisfy the relation
// against the provided constant.
//
// Some implications of this behaviour:
// - a >= b is *not* equivalent to (a > b || a == b).  e.g. a=(2,2), b=(2,1).
// - a > b does not imply a <= b (though the contrapositive *does* hold)
// - >= (and <=) are not total (in economics terms, ``complete''): (a >= b) and (b >= a) can both be
//   false.  e.g. a=(1,2), b=(2,1).
// - a == 0 tests whether a good is empty (this is, has no goods at all, or has only goods with
//   quantities of 0).
//
// Division of one Bundle (but *not* NegativeBundles) by another is also defined as follows:  a/b
// returns the minimum value m such that m*b >= a.  Modulus is defined in a related way: a % b
// equals a Bundle c such that c = (a / b) * b - a.  For example, if a=(2,3,1), b=(1,2,2.5) then a/b
// == 2 and a%b == (0,1,4).  If any good with a positive quantity in a does not have a positive
// quantity in b division returns infinity and modulus results in a Bundle with quantity infinity
// for every positive quantity good in b.  A special method covers() is provided for this purpose:
// b.covers(a) returns true iff b has positive quantities for every good in a.  Division of a
// zero-bundle by another zero-bundle return a (quiet) NaN.  Attempting to use a%b when b.covers(a)
// is false will result in a negativity_error exception (see below) from the above '- a' operation.
//
// Comparison operators w.r.t. a double are also defined, and return true if the comparison to the
// double holds for every existing quantity in the Bundle, and all are always true for an empty
// bundle.  Note that this behaviour may not do what you expect if goods that you care about aren't
// actually in the bundle.  For example, if a=(x=1,y=2) and b=(x=1,y=1,z=1), a >= 1 will be true,
// though a-b will result in a potentially unexpected negative (and illegal, if using Bundle instead
// of NegativeBundle) quantity.  If in doubt, always compare to fixed Bundle that has known
// (significant) quantities.
//
// common(a,b) returns a maximum common bundle between one bundle and another.  The resulting bundle will
// contain all goods that are in both bundles (though quantities may be zero), and will not include
// goods only in one of the two bundles.  If either a or b is a BundleNegative, any negative
// quantities will be treated as if they do not exist in the Bundle.
// e.g. this=[a=1,b=3,c=1,d=0], that=[a=4,c=1,d=4], common(this,that)=[a=1,c=1,d=0]
//
// reduce(&a,&b) is similar to common(), above, except that it also subtracts the common Bundle from
// both a and b before returning it.  Like common(), negative quantities are treated as
// non-existant.
//
// Attempting to do something to a Bundle that would induce a negative quantity for one of the
// Bundle's goods will throw a Bundle::negativity_error exception (which is a subclass of
// std::range_error).
//
// Note that these class is not aware of a Good's increment parameter; any class directly modifying
// a Bundle should thus ensure that the increment is respected.

class Bundle;
class BundleNegative {
    private:
        std::unordered_map<eris_id_t, double> bundle;
    public:
        BundleNegative() {}
        BundleNegative(eris_id_t g, double q);
        BundleNegative(std::initializer_list<std::pair<eris_id_t, double>> init);

        virtual ~BundleNegative() = default;

        virtual double operator[] (eris_id_t gid) const;
        virtual void set(eris_id_t gid, double quantity);
        virtual void set(std::initializer_list<std::pair<eris_id_t, double>> goods);
        virtual std::unordered_map<eris_id_t, double>::const_iterator begin() const;
        virtual std::unordered_map<eris_id_t, double>::const_iterator end() const;

        virtual bool empty() const;
        virtual std::unordered_map<eris_id_t, double>::size_type size() const;
        virtual int count(eris_id_t) const;
        virtual int erase(eris_id_t);
        virtual double remove(eris_id_t);
        virtual void clearZeros();

        BundleNegative& operator += (const BundleNegative &b) noexcept;
        BundleNegative& operator -= (const BundleNegative &b) noexcept;
        BundleNegative& operator *= (const double &m) noexcept;
        BundleNegative& operator /= (const double &d) noexcept;

        BundleNegative operator + (const BundleNegative &b) const noexcept;
        BundleNegative operator - (const BundleNegative &b) const noexcept;
        virtual BundleNegative operator - () const noexcept;
        BundleNegative operator * (const double &m) const noexcept;
        BundleNegative operator / (const double &d) const noexcept;
        friend BundleNegative operator * (const double &m, const BundleNegative &b) noexcept;

        bool operator >  (const BundleNegative &b) const noexcept;
        bool operator >= (const BundleNegative &b) const noexcept;
        bool operator == (const BundleNegative &b) const noexcept;
        bool operator != (const BundleNegative &b) const noexcept;
        bool operator <  (const BundleNegative &b) const noexcept;
        bool operator <= (const BundleNegative &b) const noexcept;
        bool operator >  (const double &q) const noexcept;
        bool operator >= (const double &q) const noexcept;
        bool operator == (const double &q) const noexcept;
        bool operator != (const double &q) const noexcept;
        bool operator <  (const double &q) const noexcept;
        bool operator <= (const double &q) const noexcept;
        friend bool operator >  (const double &q, const BundleNegative &b) noexcept;
        friend bool operator >= (const double &q, const BundleNegative &b) noexcept;
        friend bool operator == (const double &q, const BundleNegative &b) noexcept;
        friend bool operator != (const double &q, const BundleNegative &b) noexcept;
        friend bool operator <  (const double &q, const BundleNegative &b) noexcept;
        friend bool operator <= (const double &q, const BundleNegative &b) noexcept;

        friend class Bundle;
};

class Bundle : public BundleNegative {
    public:
        Bundle() {};
        Bundle(eris_id_t g, double q);
        Bundle(std::initializer_list<std::pair<eris_id_t, double>> init);
        using BundleNegative::set;
        virtual void set(eris_id_t gid, double quantity);

        Bundle& operator += (const BundleNegative &b);
        Bundle& operator -= (const BundleNegative &b);
        Bundle& operator *= (const double &m);
        Bundle& operator /= (const double &d);

        Bundle operator + (const BundleNegative &b) const;
        Bundle operator - (const BundleNegative &b) const;
        Bundle operator * (const double &m) const;
        friend Bundle operator * (const double &m, const Bundle &a);
        Bundle operator / (const double &d) const;

        bool covers(const Bundle &b) const noexcept;
        double operator / (const Bundle &b) const noexcept;
        Bundle operator % (const Bundle &b) const;

        static Bundle common(const BundleNegative &a, const BundleNegative &b) noexcept;
        static Bundle reduce(BundleNegative &a, BundleNegative &b) noexcept;

        class negativity_error : public std::range_error {
            public:
                static negativity_error create(eris_id_t good, double value) {
                    return negativity_error("Good " + std::to_string(good) + " assigned illegal negative value " +
                            std::to_string(value), good, value);
                }
                negativity_error(eris_id_t good, double value) :
                    std::range_error("Good " + std::to_string(good) + " assigned illegal negative value " + std::to_string(value)),
                    good(good), value(value) {}
                negativity_error(const std::string& what_arg, eris_id_t good, double value) :
                    std::range_error(what_arg), good(good), value(value) {}
                negativity_error(const char* what_arg, eris_id_t good, double value) :
                    std::range_error(what_arg), good(good), value(value) {}
                const eris_id_t good;
                const double value;
        };
};

// Define everything here, so that these functions can be inlined (if the compiler decides its
// helpful).

inline BundleNegative::BundleNegative(eris_id_t g, double q) {
    set(g, q);
}
inline BundleNegative::BundleNegative(std::initializer_list<std::pair<eris_id_t, double>> init) {
    set(init);
}
inline Bundle::Bundle(eris_id_t g, double q) {
    set(g, q);
}
inline Bundle::Bundle(std::initializer_list<std::pair<eris_id_t, double>> init) {
    BundleNegative::set(init);
}

inline double BundleNegative::operator[] (eris_id_t gid) const {
    // Don't want to invoke map's [] operator, because it auto-vivifies the element
    return count(gid) ? bundle.at(gid) : 0.0;
}
inline void BundleNegative::set(std::initializer_list<std::pair<eris_id_t, double>> goods) {
    for (auto g : goods) set(g.first, g.second);
}
inline void BundleNegative::set(eris_id_t gid, double quantity) {
    bundle[gid] = quantity;
}

inline void Bundle::set(eris_id_t gid, double quantity) {
    if (quantity < 0)
        throw negativity_error::create(gid, quantity);

    BundleNegative::set(gid, quantity);
}

inline bool BundleNegative::empty() const {
    return bundle.empty();
}
inline std::unordered_map<eris_id_t, double>::size_type BundleNegative::size() const {
    return bundle.size();
}
inline int BundleNegative::count(eris_id_t gid) const {
    return bundle.count(gid);
}
inline int BundleNegative::erase(eris_id_t gid) {
    return bundle.erase(gid);
}
inline double BundleNegative::remove(eris_id_t gid) {
    double d = operator[](gid);
    erase(gid);
    return d;
}
inline void BundleNegative::clearZeros() {
    for (auto it = bundle.cbegin(); it != bundle.cend(); ) {
        if (it->second == 0)
            bundle.erase(it++);
        else
            ++it;
    }
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::begin() const {
    return bundle.cbegin();
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::end() const {
    return bundle.cend();
}

#define __ERIS_BUNDLE_HPP_ADDSUB(OP, OPEQ)\
inline BundleNegative& BundleNegative::operator OPEQ (const BundleNegative &b) noexcept {\
    for (auto g : b.bundle) bundle[g.first] OPEQ g.second;\
    return *this;\
}\
inline BundleNegative BundleNegative::operator OP (const BundleNegative &b) const noexcept {\
    BundleNegative ret(*this);\
    ret OPEQ b;\
    return ret;\
}\
inline Bundle& Bundle::operator OPEQ (const BundleNegative &b) {\
    for (auto g : b.bundle) {\
        double q = bundle[g.first] OP g.second;\
        if (q < 0)\
            throw negativity_error(g.first, q);\
        bundle[g.first] = q;\
    }\
    return *this;\
}\
inline Bundle Bundle::operator OP (const BundleNegative &b) const {\
    Bundle ret(*this);\
    ret OPEQ b;\
    return ret;\
}

__ERIS_BUNDLE_HPP_ADDSUB(+, +=)
__ERIS_BUNDLE_HPP_ADDSUB(-, -=)

#undef __ERIS_BUNDLE_HPP_ADDSUB

inline BundleNegative& BundleNegative::operator *= (const double &m) noexcept {
    for (auto g : bundle) bundle[g.first] *= m;
    return *this;
}
inline Bundle& Bundle::operator *= (const double &m) {
    if (m < 0) throw negativity_error("Attempt to scale Bundle by negative value " + std::to_string(m), 0, m);
    return *static_cast<Bundle*>(&BundleNegative::operator*=(m));
}
inline BundleNegative& BundleNegative::operator /= (const double &d) noexcept {
    return *this *= (1.0 / d);
}
inline BundleNegative BundleNegative::operator - () const noexcept {
    return operator*(-1.0);
}
inline BundleNegative BundleNegative::operator * (const double &m) const noexcept {
    BundleNegative ret(*this);
    ret *= m;
    return ret;
}
inline BundleNegative operator * (const double &m, const BundleNegative &b) noexcept {
    return b * m;
}
inline Bundle Bundle::operator * (const double &m) const {
    Bundle ret(*this);
    ret *= m;
    return ret;
}
inline Bundle operator * (const double &m, const Bundle &b) {
    return b * m;
}
inline BundleNegative BundleNegative::operator / (const double &d) const noexcept {
    return *this * (1.0/d);
}
inline Bundle Bundle::operator / (const double &d) const {
    return *this * (1.0/d);
}

inline bool Bundle::covers(const Bundle &b) const noexcept {
    for (auto g : b)
        if ((*this)[g.first] <= 0) return false;
    return true;
}
inline double Bundle::operator / (const Bundle &b) const noexcept {
    double mult = 0;
    for (auto g : bundle) {
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
inline Bundle Bundle::operator % (const Bundle &b) const {
    Bundle ret = (*this / b) * b;
    ret -= *this;
    ret.clearZeros();
    return ret;
}

inline Bundle Bundle::common(const BundleNegative &a, const BundleNegative &b) noexcept {
    Bundle result;
    for (auto ag : a) {
        if (ag.second >= 0 and b.count(ag.first)) {
            double bq = b[ag.first];
            if (bq >= 0) result.set(ag.first, std::min<double>(ag.second, bq));
        }
    }
    return result;
}

inline Bundle Bundle::reduce(BundleNegative &a, BundleNegative &b) noexcept {
    Bundle result = common(a, b);
    a -= result;
    b -= result;
    return result;
}

#define __ERIS_BUNDLE_HPP_COMPARE(OP) \
inline bool BundleNegative::operator OP (const BundleNegative &b) const noexcept {\
    std::unordered_set<eris_id_t> goods;\
    for (auto g : bundle) goods.insert(goods.end(), g.first);\
    for (auto g : b.bundle) goods.insert(g.first);\
\
    for (auto g : goods)\
        if (!((*this)[g] OP b[g])) return false;\
    return true;\
}\
inline bool BundleNegative::operator OP (const double &q) const noexcept {\
    for (auto g : bundle)\
        if (!(g.second OP q)) return false;\
    return true;\
}\
inline bool operator OP (const double &q, const BundleNegative &b) noexcept {\
    return b OP q;\
}

__ERIS_BUNDLE_HPP_COMPARE(==)
__ERIS_BUNDLE_HPP_COMPARE(<)
__ERIS_BUNDLE_HPP_COMPARE(<=)
__ERIS_BUNDLE_HPP_COMPARE(>)
__ERIS_BUNDLE_HPP_COMPARE(>=)

#undef __ERIS_BUNDLE_HPP_COMPARE

inline bool BundleNegative::operator != (const BundleNegative &b) const noexcept {
    return !(*this == b);
}
inline bool BundleNegative::operator != (const double &q) const noexcept {
    return !(*this == q);
}
inline bool operator != (const double &q, const BundleNegative &b) noexcept {
    return b != q;
}

}
