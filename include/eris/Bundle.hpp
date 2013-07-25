#pragma once
#include <eris/types.hpp>
#include <algorithm>
#include <exception>
#include <limits>
#include <ostream>
#include <unordered_map>

namespace eris {

/**
 * \class BundleNegative
 * \brief A BundleNegative represents a set of goods with quantities, which may be negative.
 *
 * \class Bundle
 * \brief A Bundle represents a set of goods with each good containing a non-negative quantity.
 *
 * Accessing quantities is done through the [] operation (e.g. bundle[gid]).  Accessing a good that
 * is not contained in the Bundle returns a value of 0.
 *
 * Setting values is done through the set(id, value) method, *not* using the [] operator (since
 * assignment may require checking, e.g. for positive quantities).
 *
 * You can iterate through goods via the usual begin()/end() pattern; note that these get mapped to
 * through to the underlying std::unordered_map<eris_id_t,double>'s cbegin()/cend() methods, and so
 * are immutable.
 *
 * The usual +, -, *, / operators as overloaded as expected for adding/scaling bundles, plus the
 * analogous +=, -=, *=, and /= operators.  After addition or subtraction, the result will contain
 * all goods that existed in either good, even if those goods had quantities of 0.  Unary negative
 * is defined, but always returns a BundleNegative (even for a Bundle).  Adding and subtracting two
 * Bundles returns a Bundle; if either of the bundles being added or subtracted are BundleNegative
 * objects, a BundleNegative is returned.
 *
 * Comparison operators are also overloaded.  Each of ==, >, >=, <, and <= is true iff all the
 * inequality relation is satisfied for the quantities of every good in either bundle.  Note that !=
 * is also overloaded, but different from the above: a != b is equivalent to !(a == b).  Goods that
 * are missing from one bundle or the other are implicitly assumed to have value 0.  Comparison can
 * also be done against a constant in which case the quantity of each good must satisfy the relation
 * against the provided constant (again except for !=, which is the negation of ==).
 *
 * Some implications of this behaviour:
 * - a >= b is *not* equivalent to (a > b || a == b).  e.g. a=(2,2), b=(2,1).
 * - a > b does not imply a <= b (though the contrapositive *does* hold)
 * - >= (and <=) are not total (in economics terms, ``complete''): (a >= b) and (b >= a) can both be
 *   false.  e.g. a=(1,2), b=(2,1).
 * - a == 0 tests whether a good is empty (this is, has no goods at all, or has only goods with
 *   quantities of 0).
 *
 *
 * Comparison operators w.r.t. a double are also defined, and return true if the comparison to the
 * double holds for every existing quantity in the Bundle, and all are always true for an empty
 * bundle.  Note that this behaviour may not do what you expect if goods that you care about aren't
 * actually in the bundle.  For example, if a=(x=1,y=2) and b=(x=1,y=1,z=1), a >= 1 will be true,
 * though a-b will result in a potentially unexpected negative (and illegal, if using Bundle instead
 * of NegativeBundle) quantity.  If in doubt, always compare to fixed Bundle that has known
 * (significant) quantities.
 *
 * Attempting to do something to a Bundle that would induce a negative quantity for one of the
 * Bundle's goods will throw a Bundle::negativity_error exception (which is a subclass of
 * std::range_error).
 *
 * Note that these class is not aware of a Good's increment parameter; any class directly modifying
 * a Bundle should thus ensure that the increment is respected.
 */

class Bundle;
class BundleNegative {
    protected:
        typedef std::initializer_list<std::pair<eris_id_t, double>> init_list;

    public:
        /// Constructs a new BundleNegative with no initial good/quantity values.
        BundleNegative();
        /// Constructs a new BundleNegative containing a single good, g, with quantity q.
        BundleNegative(const eris_id_t &g, const double &q);
        /** Allows initialization with a static std::initializer_list of std::pairs, such as:
         * BundleNegative b {{1, 1.0}, {2, 0.5}, {3, 100}};
         * Since an initializer_list requires constant values, this is primary useful for debugging
         * purposes.
         */
        BundleNegative(const init_list &init);

        virtual ~BundleNegative() = default;

        /** Read-only access to BundleNegative quantities given a good id. */
        double operator[] (const eris_id_t &gid) const;
        /** Sets the quantity of the given good id to the given value. */
        virtual void set(const eris_id_t &gid, const double &quantity);
        std::unordered_map<eris_id_t, double>::const_iterator begin() const;
        std::unordered_map<eris_id_t, double>::const_iterator end() const;

        /** Returns the number of goods in the bundle.  Note that values that have not been
         * explicitly set (and thus return a value of 0) are *not* included in the size(), but
         * values that have been explitly set (even to 0) *are* included.
         */
        std::unordered_map<eris_id_t, double>::size_type size() const;

        /** Returns true iff size() == 0.  Note that empty() is not true for a bundle with explicit
         * quantities of 0; for testing whether a bundle is empty in the sense of all quantities
         * being 0 (explicitly or implicitly by omission), use the (b == 0) operator, discussed
         * below.
         */
        bool empty() const;

        /** Returns 1 if the given id exists in the Bundle (even if it equals 0), 0 otherwise.
         */
        int count(const eris_id_t &gid) const;

        /** Removes the specified good from the bundle (if it exists), and returns either 0 or 1
         * indicating whether the good was present in the bundle, like std::unordered_map::erase.
         */
        int erase(const eris_id_t &gid);

        /** Like erase(id), but returns the quantity of the removed good, or 0 if the good was not
         * in the bundle.  Note that there is no way to distinguish between a good that was not in
         * the Bundle and a good that was in the Bundle with a quantity of 0.
         */
        double remove(const eris_id_t &gid);

        /** Removes any goods from the bundle that have a quantity equal to 0. */
        void clearZeros();

        /** Removes all goods/quantities from the Bundle. */
        void clear();

        /** Constructs a new Bundle consisting of all the strictly positive quantities of this
         * BundleNegative.  Note that goods with a quantity of 0 are not included. */
        Bundle positive() const noexcept;

        /** Constructs a new Bundle consisting of all the strictly negative quantities on this
         * BundleNegative, converted to positive values.  Note that goods with a quantity of 0 are
         * not included.
         *
         * For example:
         *
         *     BundleNegative new {{1, -2}, {2, 1}, {3, 0}};
         *     new.negative() == Bundle {{1, 2}, {3, 0}};
         *
         * This is equivalent to (-bundle).positive(), but more efficient.
         */
        Bundle negative() const noexcept;

        /** Returns all goods with quantities of 0.  Complementary to positive() and negative().
         */
        Bundle zeros() const noexcept;

        BundleNegative& operator += (const BundleNegative &b);
        BundleNegative& operator -= (const BundleNegative &b);
        BundleNegative& operator *= (const double &m);
        BundleNegative& operator /= (const double &d);

        /// The default epsilon for transferApprox(), if not specified.
        static constexpr double default_transfer_epsilon = 1.0e-14;

        /** Transfers (approximately) the given amount between two Bundles.  Positive quantities in
         * `amount` are transferred from the invoked object to the `to` Bundle; negative quantities
         * are transferred from the `to` Bundle to the invoked object.  The error parameter is a
         * relative amount that defines how close values should get before treating them as zero
         * quantities.
         *
         * Calling
         *
         *     from.transferApprox(amount, to);
         *
         * is roughly equivalent to
         *
         *     from -= amount;
         *     to += amount;
         *
         * except for the error tolerance handling.
         *
         * The error handling is particularly useful to avoid problems with numerical imprecision
         * resulting in potentially negative quantities in Bundles.  In particular it specially
         * does two things:
         *
         * - If a transfer would result in a good in the source Bundle having a quantity with
         *   absolute value less than `epsilon` times the pre-transfer quantity, the transferred
         *   quantity of that good will instead be the total quantity in the source Bundle.
         * - Otherwise, if a transfer would results in a good in the destination Bundle having a quantity with
         *   absolute value less than `epsilon` times the pre-transfer quantity, the transferred
         *   quantity will be the amount required to reach exactly 0.  (Note that this case is only
         *   possible when the destination is a BundleNegative with a negative quantity, since the
         *   destination bundle is always the one being added to).
         */
        void transferApprox(const BundleNegative &amount, BundleNegative &to, double epsilon = default_transfer_epsilon);

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

        /** Overloaded so that a Bundle can be printed nicely with `std::cout << bundle`.
         *
         * Example outputs:
         *
         *     Bundle([4]=12, [6]=0)
         *     Bundle()
         *     BundleNegative([3]=-3.75, [2]=0, [1]=4.2e-24)
         *
         * The value in brackets is the eris_id_t of the good.
         */
        friend std::ostream& operator << (std::ostream &os, const BundleNegative& b);

    protected:
        /// Internal method used for bundle printing.
        void _print(std::ostream &os) const;
    private:
        friend class Bundle;
        std::unordered_map<eris_id_t, double> goods_;
};

class Bundle final : public BundleNegative {
    public:
        Bundle();
        Bundle(const eris_id_t &g, const double &q);
        Bundle(const init_list &init);
        using BundleNegative::set;
        void set(const eris_id_t &gid, const double &quantity) override;

        Bundle& operator *= (const double &m);
        Bundle& operator /= (const double &d);

        // Inherit the BundleNegative versions of these:
        using BundleNegative::operator +;
        using BundleNegative::operator -;
        /** Adding two Bundle objects returns a Bundle object.  Note that it is the visible type,
         * not the actual type, that governs this behaviour.  In other words, in the following code
         * `c` will be a Bundle and `d` will be a BundleNegative:
         *
         *     Bundle a = ...;
         *     Bundle b = ...;
         *     auto c = a + b;
         *     auto d = a + (BundleNegative) b;
         */
        Bundle operator + (const Bundle &b) const noexcept;
        /** Subtracting two Bundle objects returns a Bundle object.  Note that this can throw an
         * exception if the quantity any good in b is greater than in a.  If you want to allow for
         * the quantity to be negative, you should cast one of the objects to a BundleNegative or
         * use unary minus (which returns a BundleNegative), such as one of these:
         *
         *     Bundle a({{ 1, 2 }});
         *     Bundle b({{ 1, 3 }});
         *
         *     auto pos1 = b - a;                  // Bundle([1]=1)
         *     auto neg1 = (BundleNegative) a - b; // BundleNegative([1]=-1)
         *     auto neg2 = -b + a;                 // Same
         *     auto neg3 = a - b; // Oops: throws eris::Bundle::negativity_error
         *
         * The neg1 calculation is slightly more efficient than neg2 as it doesn't need to create
         * the intermediate `-b` BundleNegative object.
         */
        Bundle operator - (const Bundle &b) const;
        /** Multiplying a Bundle by a constant returns a Bundle with quantities scaled by the
         * constant.
         *
         * \throws Bundle::negativity_error if m < 0.  If you need a negative, first cast the Bundle
         * as a BundleNegative, such as: `auto neg = -3 * (BundleNegative) b;`
         */
        Bundle operator * (const double &m) const;
        friend Bundle operator * (const double &m, const Bundle &a);
        /** Dividing a Bundle by a constant returns a Bundle with quantities scaled by the inverse
         * of the constant.
         *
         * \throws Bundle::negativity_error if m < 0
         */
        Bundle operator / (const double &d) const;

        /** Performs "bundle coverage," returning the multiples of the provided bundle needed to
         * cover the called-upon Bundle.  Coverage of a Bundle (but *not* BundleNegative) by another
         * is also defined as follows:  `a.coverage(b)` returns the minimum value \f$m\f$ such that
         * \f$mb \gtreq a\f$.
         *
         * Coverage of a zero-bundle by another zero-bundle return a (quiet) NaN.  Coverage of a
         * Bundle with a positive quantity in one or more goods by a Bundle with a quantity of 0 for
         * any of those goods returns infinity.
         *
         * For example, if \f$a = (2,3,1), b = (1,2,2.5)\f$ then `a.coverage(b)` returns 2.
         *
         * multiples() is a related operator that returns the largest multiple of b doesn't exceed a.
         *
         * \sa coverageExcess
         * \sa multiples
         * \sa covers
         */
        double coverage(const Bundle &b) const noexcept;
        /** Returns the Bundle of "leftover", excess quantities that would result from a coverage()
         * call.  Mathematically, these two quantities are equal:
         *
         *     b * a.coverage(b);
         *     a + a.coverageExcess(b);
         *
         * Conceptually, this returns the excess of using multiples of Bundle b to produce Bundle a.
         *
         * Calling this in cases where coverage() returns infinity will throw a negativity_error
         * exception.  covers() can be used to detect this case.
         *
         * Assuming an exception is not thrown, this method is equivalent to `b * a.coverage(b) -
         * a`, but also clears zero values from the resulting Bundle.
         *
         * For example, if \f$a = (2,3,1), b = (1,2,2.5)\f$ then \f$a \% b = (0,1,4)\f$
         *
         * \sa coverage
         * \sa covers
         * \sa multiples
         */
        Bundle coverageExcess(const Bundle &b) const;
        /** Returns true iff the current Bundle has positive quantities for every positive-quantity
         * good in the passed-in Bundle b.
         *
         * Example:
         *
         *     Bundle a {{1,3}, {2, 4}};
         *     Bundle b {{1,1}, {2, 1}, {3,1}};
         *     Bundle c {{1,0}, {2, 100}, {3, 1000}};
         *
         *     a.covers(b); // FALSE: a has no quantity of good 3
         *     a.covers(c); // FALSE: no quantity of 3
         *     b.covers(a); // TRUE: b has positive quantities of every good in a
         *     b.covers(c); // TRUE: same
         *     c.covers(a); // FALSE: a has no (positive) quantity of good 1
         *     c.covers(b); // FALSE: c does not have positive quantities of good 1, but b does
         */
        bool covers(const Bundle &b) const noexcept;

        /** Returns the number of multiples of b that are contained in the current Bundle.
         * Mathematically, `a.multiples(b)` returns the largest value \f$m\f$ such that \f$a \gtreq
         * mb\f$.  If both are zero bundles, returns (quiet) NaN.
         *
         * This is intended to answer the question "How many multiples of b can be created from a?",
         * while Bundle coverage answers "How many multiples of b does it take to have a Bundle at
         * least as large as a?"
         *
         * Note: this is equivalent to the numerical inverse of reversed Bundle coverage, i.e.
         * `a.multiples(b) == 1.0 / (b.coverage(a))`, but is slightly more efficient, particularly
         * when b contains substantially fewer goods than a.
         *
         * Example:
         *
         *     Bundle a {{1,100}, {2,10}};
         *     Bundle b {         {2,1}};
         *     Bundle c {{1,5}};
         *     a.coverage(b);  // Infinity
         *     b.coverage(a);  // 0.1
         *     a.multiples(b); // 10
         *     b.multiples(a); // 0
         *     a.coverage(c);  // Infinity
         *     c.coverage(a);  // 0.05
         *     a.multiples(c); // 20
         *     c.multiples(a); // 0
         */

        double multiples(const Bundle &b) const noexcept;

        /** Returns a maximum common bundle between one bundle and another.  The resulting bundle
         * will contain all goods that are in both bundles (though quantities may be zero), and will
         * not include goods only in one of the two bundles.  The quantity of each good will be the
         * lower quantity of the good in the two bundles.  Any negative quantities will be treated
         * as if they do not exist.
         *
         * Example: if \f$b_1=[a=1, b=3, c=1, d=0, e=-3], b_2=[a=4, c=1, d=4, e=2]\f$ then
         * \f$common(b_1,b_2) = [a=1,c=1,d=0]\f$.
         *
         * \see reduce(BundleNegative&, BundleNegative&)
         */
        static Bundle common(const BundleNegative &a, const BundleNegative &b) noexcept;
        /** This is like common(), but it also subtracts the common Bundle from both bundles before
         * returning it.
         */
        static Bundle reduce(BundleNegative &a, BundleNegative &b);

        /** Overloaded so that a Bundle can be printed nicely with `std::cout << bundle`.
         *
         * Example outputs:
         *
         *     Bundle([4]=12, [6]=0)
         *     Bundle()
         *     BundleNegative([3]=-3.75, [2]=0, [1]=4.2e-24)
         *
         * The value in brackets is the eris_id_t of the good.
         */
        friend std::ostream& operator << (std::ostream &os, const Bundle& b);

        /** Exception class for attempting to do some operation that would resulting in a negative
         * quantity being set in a Bundle.
         */
        class negativity_error : public std::range_error {
            public:
                negativity_error(const eris_id_t &good, const double &value) :
                    std::range_error("eris_id_t=" + std::to_string(good) + " assigned illegal negative value "
                            + std::to_string(value) + " in Bundle."), good(good), value(value) {}
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
inline BundleNegative::BundleNegative() {}
inline BundleNegative::BundleNegative(const eris_id_t &g, const double &q) { set(g, q); }
inline Bundle::Bundle() {}
inline Bundle::Bundle(const eris_id_t &g, const double &q) { set(g, q); }

inline double BundleNegative::operator[] (const eris_id_t &gid) const {
    // Don't want to invoke map's [] operator, because it auto-vivifies the element
    return count(gid) ? goods_.at(gid) : 0.0;
}
inline void BundleNegative::set(const eris_id_t &gid, const double &quantity) {
    goods_[gid] = quantity;
}

inline void Bundle::set(const eris_id_t &gid, const double &quantity) {
    if (quantity < 0) throw negativity_error(gid, quantity);
    BundleNegative::set(gid, quantity);
}

inline bool BundleNegative::empty() const {
    return goods_.empty();
}
inline std::unordered_map<eris_id_t, double>::size_type BundleNegative::size() const {
    return goods_.size();
}
inline int BundleNegative::count(const eris_id_t &gid) const {
    return goods_.count(gid);
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::begin() const {
    return goods_.cbegin();
}
inline std::unordered_map<eris_id_t, double>::const_iterator BundleNegative::end() const {
    return goods_.cend();
}

#define _ERIS_BUNDLE_HPP_ADDSUB(OP, OPEQ)\
inline BundleNegative& BundleNegative::operator OPEQ (const BundleNegative &b) {\
    for (auto g : b.goods_) set(g.first, (*this)[g.first] OP g.second);\
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
    for (auto g : goods_) set(g.first, g.second * m);
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
