#pragma once
#include <eris/types.hpp>
#include <algorithm>
#include <exception>
#include <limits>
#include <ostream>
#include <set>
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
// all goods that existed in either good, even if those goods had quantities of 0.  Unary negative
// is defined, but always returns a BundleNegative (even for a Bundle).  Adding and subtracting two
// Bundles returns a Bundle; if either of the bundles being added or subtracted are BundleNegative
// objects, a BundleNegative is returned.
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
// quantity in b division returns infinity and modulus throws a negativity error.  A special method
// covers() is provided for this purpose: b.covers(a) returns true iff b has positive quantities for
// every positive-quantity good in a.  Division of a zero-bundle by another zero-bundle return a
// (quiet) NaN.  Attempting to use a%b when b.covers(a) is false will result in a negativity_error
// exception (see below) from the above '- a' operation.
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
        typedef std::initializer_list<std::pair<eris_id_t, double>> init_list;
    public:
        BundleNegative() {}
        BundleNegative(const eris_id_t &g, const double &q);
        BundleNegative(const init_list &init);

        virtual ~BundleNegative() = default;

        double operator[] (const eris_id_t &gid) const;
        virtual void set(const eris_id_t &gid, const double &quantity);
        virtual void set(const init_list &goods);
        std::unordered_map<eris_id_t, double>::const_iterator begin() const;
        std::unordered_map<eris_id_t, double>::const_iterator end() const;

        bool empty() const;
        std::unordered_map<eris_id_t, double>::size_type size() const;
        int count(const eris_id_t &gid) const;
        int erase(const eris_id_t &gid);
        double remove(const eris_id_t &gid);
        void clearZeros();

        BundleNegative& operator += (const BundleNegative &b);
        BundleNegative& operator -= (const BundleNegative &b);
        BundleNegative& operator *= (const double &m);
        BundleNegative& operator /= (const double &d);

        BundleNegative operator + (const BundleNegative &b) const noexcept;
        BundleNegative operator - (const BundleNegative &b) const noexcept;
        BundleNegative operator - () const noexcept;
        BundleNegative operator * (const double &m) const noexcept;
        BundleNegative operator / (const double &d) const noexcept;
        friend BundleNegative operator * (const double &m, const BundleNegative &b) noexcept;

        // Bundle <-> Bundle comparisons:
        bool operator >  (const BundleNegative &b) const noexcept;
        bool operator >= (const BundleNegative &b) const noexcept;
        bool operator == (const BundleNegative &b) const noexcept;
        bool operator != (const BundleNegative &b) const noexcept;
        bool operator <  (const BundleNegative &b) const noexcept;
        bool operator <= (const BundleNegative &b) const noexcept;

        // Bundle <-> constant comparisons:
        bool operator >  (const double &q) const noexcept;
        bool operator >= (const double &q) const noexcept;
        bool operator == (const double &q) const noexcept;
        bool operator != (const double &q) const noexcept;
        bool operator <  (const double &q) const noexcept;
        bool operator <= (const double &q) const noexcept;

        // constant <-> Bundle comparisons:
        friend bool operator >  (const double &q, const BundleNegative &b) noexcept;
        friend bool operator >= (const double &q, const BundleNegative &b) noexcept;
        friend bool operator == (const double &q, const BundleNegative &b) noexcept;
        friend bool operator != (const double &q, const BundleNegative &b) noexcept;
        friend bool operator <  (const double &q, const BundleNegative &b) noexcept;
        friend bool operator <= (const double &q, const BundleNegative &b) noexcept;

        // For printing a bundle
        friend std::ostream& operator << (std::ostream &os, const BundleNegative& b);

        friend class Bundle;
};

class Bundle : public BundleNegative {
    public:
        Bundle() {};
        Bundle(const eris_id_t &g, const double &q);
        Bundle(const init_list &init);
        using BundleNegative::set;
        void set(const eris_id_t &gid, const double &quantity) override;

        Bundle& operator *= (const double &m);
        Bundle& operator /= (const double &d);

        // Inherit the BundleNegative versions of these:
        using BundleNegative::operator +;
        using BundleNegative::operator -;
        // Also define +/- for two Bundles to return a Bundle (instead of a BundleNegative)
        Bundle operator + (const Bundle &b) const noexcept;
        Bundle operator - (const Bundle &b) const;
        Bundle operator * (const double &m) const;
        friend Bundle operator * (const double &m, const Bundle &a);
        Bundle operator / (const double &d) const;

        bool covers(const Bundle &b) const noexcept;
        double operator / (const Bundle &b) const noexcept;
        Bundle operator % (const Bundle &b) const;

        static Bundle common(const BundleNegative &a, const BundleNegative &b) noexcept;
        static Bundle reduce(BundleNegative &a, BundleNegative &b);

        // For printing a bundle
        friend std::ostream& operator << (std::ostream &os, const Bundle& b);

        class negativity_error : public std::range_error {
            public:
                negativity_error(const eris_id_t &good, const double &value) :
                    std::range_error("Good " + std::to_string(good) + " assigned illegal negative value "
                            + std::to_string(value)), good(good), value(value) {}
                negativity_error(const std::string& what_arg, const eris_id_t &good, const double &value) :
                    std::range_error(what_arg), good(good), value(value) {}
                negativity_error(const char* what_arg, const eris_id_t &good, const double &value) :
                    std::range_error(what_arg), good(good), value(value) {}
                const eris_id_t good;
                const double value;
        };
};

// Define various methods that are frequently called here, so that these functions can be inlined
// (if the compiler decides it's helpful).  Less frequently called and/or more complex functions are
// in src/Bundle.cpp

inline BundleNegative::BundleNegative(const eris_id_t &g, const double &q) {
    set(g, q);
}
inline BundleNegative::BundleNegative(const init_list &init) {
    set(init);
}
inline Bundle::Bundle(const eris_id_t &g, const double &q) {
    set(g, q);
}
inline Bundle::Bundle(const init_list &init) {
    BundleNegative::set(init);
}

inline double BundleNegative::operator[] (const eris_id_t &gid) const {
    // Don't want to invoke map's [] operator, because it auto-vivifies the element
    return count(gid) ? bundle.at(gid) : 0.0;
}
inline void BundleNegative::set(const init_list &goods) {
    for (auto g : goods) set(g.first, g.second);
}
inline void BundleNegative::set(const eris_id_t &gid, const double &quantity) {
    bundle[gid] = quantity;
}

inline void Bundle::set(const eris_id_t &gid, const double &quantity) {
    if (quantity < 0)
        throw negativity_error(gid, quantity);

    BundleNegative::set(gid, quantity);
}

inline bool BundleNegative::empty() const {
    return bundle.empty();
}
inline std::unordered_map<eris_id_t, double>::size_type BundleNegative::size() const {
    return bundle.size();
}
inline int BundleNegative::count(const eris_id_t &gid) const {
    return bundle.count(gid);
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::begin() const {
    return bundle.cbegin();
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::end() const {
    return bundle.cend();
}

#define _ERIS_BUNDLE_HPP_ADDSUB(OP, OPEQ)\
inline BundleNegative& BundleNegative::operator OPEQ (const BundleNegative &b) {\
    for (auto g : b.bundle) set(g.first, (*this)[g.first] OP g.second);\
    return *this;\
}\
inline BundleNegative BundleNegative::operator OP (const BundleNegative &b) const noexcept {\
    BundleNegative ret(*this);\
    ret OPEQ b;\
    return ret;\
}

_ERIS_BUNDLE_HPP_ADDSUB(+, +=)
_ERIS_BUNDLE_HPP_ADDSUB(-, -=)

#undef _ERIS_BUNDLE_HPP_ADDSUB

inline Bundle Bundle::operator + (const Bundle &b) const noexcept {
    Bundle ret(*this);
    ret += b;
    return ret;
}
inline Bundle Bundle::operator - (const Bundle &b) const {
    Bundle ret(*this);
    ret -= b;
    return ret;
}


inline BundleNegative& BundleNegative::operator *= (const double &m) {
    for (auto g : bundle) set(g.first, g.second * m);
    return *this;
}
inline Bundle& Bundle::operator *= (const double &m) {
    if (m < 0) throw negativity_error("Attempt to scale Bundle by negative value " + std::to_string(m), 0, m);
    BundleNegative::operator*=(m);
    return *this;
}
inline BundleNegative& BundleNegative::operator /= (const double &d) {
    return *this *= (1.0 / d);
}
inline Bundle& Bundle::operator /= (const double &d) {
    if (d < 0) throw negativity_error("Attempt to scale Bundle by negative value 1/" + std::to_string(d), 0, d);
    BundleNegative::operator/=(d);
    return *this;
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

}
