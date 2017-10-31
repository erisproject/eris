#pragma once
#include <eris/types.hpp>
#include <stdexcept>
#include <ostream>
#include <unordered_map>
#include <forward_list>
#include <initializer_list>
#include <string>
#include <utility>

namespace eris {

/**
 * \class BundleSigned
 * \brief A BundleSigned represents a set of goods with quantities, which may be negative.
 *
 * \class Bundle
 * \brief A Bundle represents a set of goods with each good containing a non-negative quantity.
 *
 * Accessing and modifying quantities is done through the operator (e.g. `bundle[gid]`).  Accessing
 * a good that is not contained in the Bundle returns a value of 0.
 *
 * Setting values is done either through the \ref set() method or the [] operator (which is a proxy
 * object calling set() internally, to verify values, i.e. positive quantities when needed).
 *
 * You can iterate through goods via the usual begin() / end() pattern; note that these get mapped
 * to through to the underlying std::unordered_map<eris::id_t,double>'s cbegin() and cend() methods,
 * and so are immutable.
 *
 * The usual `+`, `-`, `*`, `/` operators are overloaded as expected for adding/scaling bundles,
 * plus the analogous `+=`, `-=`, `*=`, and `/=` operators.  After addition or subtraction, the
 * result will contain all goods that existed in either good, even if those goods had quantities of
 * 0.  Unary negative is defined, but always returns a BundleSigned (even for a Bundle).  Adding
 * and subtracting two Bundles returns a Bundle; if either of the bundles being added or subtracted
 * are BundleSigned objects, a BundleSigned is returned.
 *
 * Comparison operators are also overloaded.  Each of `==`, `>`, `>=`, `<`, and `<=` returns true
 * iff all the inequality relation is satisfied for the quantities of every good in either bundle.
 * Goods that are missing from one bundle or the other are implicitly assumed to have value 0.  Note
 * that `!=` is also overloaded, but different from the above: a != b is equivalent to !(a == b).
 * Comparison can also be done against a constant in which case the quantity of each good must
 * satisfy the relation against the provided constant (again except for `!=`, which is still the
 * negation of `==`).
 *
 * Some implications of this behaviour:
 * - `a >= b` is *not* equivalent to `(a > b || a == b)`.  For example:
 *       Bundle a {{1, 2}, {2, 2}};
 *       Bundle b {{1, 2}, {2, 1}};
 *       a >= b; // TRUE
 *       a > b;  // FALSE
 *       a == b; // FALSE
 * - `!(a > b)` is not equivalent to `a <= b`.
 * - `>=` (and `<=`) are not total (in economics terms, "complete"): `(a >= b)` and `(b >= a)` can
 *   both be false.  For example,  a=(1,2), b=(2,1).
 * - `a == 0` tests whether a good is "empty" in the sense of having no non-zero quantities.  This
 *   is different from the empty() method, which returns true if the Bundle has no quantities at all
 *   (not even ones with a value of 0).
 *
 * Comparison operators w.r.t. a double are also defined, and return true if the comparison to the
 * double holds for every existing quantity in the Bundle, and are always true for an empty bundle.
 * Note that this behaviour may not do what you expect if goods that you care about aren't actually
 * in the bundle.  For example, if a=(x=1,y=2) and b=(x=1,y=1,z=1), a >= 1 will be true, though a-b
 * will result in a potentially unexpected negative (and illegal, if using Bundle instead of
 * BundleSigned) quantity.  If in doubt, always compare to fixed Bundle that has known
 * (significant) quantities.
 *
 * Attempting to do something to a Bundle that would induce a negative quantity for one of the
 * Bundle's goods will throw a Bundle::negativity_error exception (which is a subclass of
 * std::range_error).
 *
 * Note that these class is not aware of any Good class restrictions: it is best thought of a simple
 * container for id-value pairs with appropriate mathematical and logical operators defined.  Thus
 * any Good quantity restrictions must take place of any class using a Bundle.
 */

class Bundle;
class BundleSigned {
    protected:
        class valueproxy; // Predeclaration
    public:
        /// Constructs a new BundleSigned with no initial good/quantity values.
        BundleSigned();

        /// Constructs a new BundleSigned containing a single good, g, with quantity q.
        BundleSigned(MemberID g, double q);

        /** Allows initialization with a static std::initializer_list of std::pairs, such as:
         * BundleSigned b {{1, 1.0}, {2, 0.5}, {3, 100}};
         * Since an initializer_list requires constant values, this is primary useful for debugging
         * purposes.
         */
        BundleSigned(const std::initializer_list<std::pair<id_t, double>> &init);

        /** Creates a new Bundle by copying quantities from another Bundle.
         *
         * Note that this differs from a default copy constructor: it does *not* copy the
         * transaction state, but rather only copies the current transaction quantities.
         */
        BundleSigned(const BundleSigned &b);

        /** Move constructor.  Unlike the copy constructor, this preserves the transaction state. */
        BundleSigned(BundleSigned &&b) = default;

        /** Assigns the values of the given Bundle to the current Bundle.
         *
         * If the source Bundle is in a transaction, only the currently-visible values are copied;
         * the transaction state and underlying values are not.  If the current Bundle is in a
         * transaction, the assigned values become part of the transaction values.
         */
        BundleSigned& operator = (const BundleSigned &b);

        virtual ~BundleSigned() = default;

        /** Read-only access to BundleSigned quantities given a good id.  Note that this does not
         * autovivify goods that don't exist in the bundle (unlike std::map's `operator[]`). */
        const double& operator[] (MemberID gid) const;

        /** Modifiable access to BundleSigned quantities given a good id.  This returns a proxy
         * object that can be used to adjust the value by internally calling set() for any
         * adjustments (to allow for value checking as needed).
         */
        valueproxy operator[] (MemberID gid);

        /** Sets the quantity of the given good id to the given value. */
        virtual void set(MemberID gid, double quantity);

        /** This method is is provided to be able to use a Bundle in a range for loop; it is, however, a
         * const_iterator, mapped internally to the underlying std::unordered_map's cbegin() method.
         */
        std::unordered_map<id_t, double>::const_iterator begin() const;

        /** This method is is provided to be able to use a Bundle in a range for loop; it is, however, a
         * const_iterator, mapped internally to the underlying std::unordered_map's cend() method.
         */
        std::unordered_map<id_t, double>::const_iterator end() const;

        /** Returns the number of goods in the bundle.  Note that values that have not been
         * explicitly set (and thus return a value of 0) are *not* included in the size(), but
         * values that have been explitly set (even to 0) *are* included.
         */
        std::unordered_map<id_t, double>::size_type size() const;

        /** Returns true iff size() == 0.  Note that empty() is not true for a bundle with explicit
         * quantities of 0; for testing whether a bundle is empty in the sense of all quantities
         * being 0 (explicitly or implicitly by omission), use the `b == 0` comparison.
         */
        bool empty() const;

        /** Returns 1 if the given id exists in the Bundle (even if it equals 0), 0 otherwise.
         */
        int count(MemberID gid) const;

        /** Removes the specified good from the bundle (if it exists), and returns either 0 or 1
         * indicating whether the good was present in the bundle, like std::unordered_map::erase.
         *
         * \sa remove
         */
        int erase(MemberID gid);

        /** Like erase(id), but returns the quantity of the removed good, or 0 if the good was not
         * in the bundle.  Note that there is no way to distinguish between a good that was not in
         * the Bundle and a good that was in the Bundle with a quantity of 0.
         */
        double remove(MemberID gid);

        /** Removes any goods from the bundle that have a quantity equal to 0. */
        void clearZeros();

        /** Removes all goods/quantities from the Bundle. */
        void clear();

        /** Constructs a new Bundle consisting of all the strictly positive quantities of this
         * BundleSigned.  Note that goods with a quantity of 0 are not included. */
        Bundle positive() const noexcept;

        /** Constructs a new Bundle consisting of all the strictly negative quantities on this
         * BundleSigned, converted to positive values.  Note that goods with a quantity of 0 are
         * not included.
         *
         * For example:
         *
         *     BundleSigned new {{1, -2}, {2, 1}, {3, 0}};
         *     new.negative() == Bundle {{1, 2}, {3, 0}};
         *
         * This is equivalent to (-bundle).positive(), but more efficient.
         */
        Bundle negative() const noexcept;

        /** Returns all goods with quantities of 0.  Complementary to positive() and negative().
         */
        Bundle zeros() const noexcept;

        /// Adds the values of one BundleSigned to the current BundleSigned.
        BundleSigned& operator += (const BundleSigned &b);
        /// Subtracts the values of one BundleSigned from the current BundleSigned.
        BundleSigned& operator -= (const BundleSigned &b);
        /// Scales a BundleSigned's quantites by `m`
        BundleSigned& operator *= (double m);
        /// Scales a BundleSigned's quantities by `1/d`
        BundleSigned& operator /= (double d);

        /// The default epsilon for transfer() and hasApprox(), if not specified.
        static constexpr double default_transfer_epsilon = 1.0e-12;

        /** Transfers (approximately) the given amount between two Bundles.  Positive quantities in
         * `amount` are transferred from the invoked object to the `to` Bundle; negative quantities
         * are transferred from the `to` Bundle to the invoked object.  The epsilon parameter is the
         * amount, relative to initial bundle quantities, that the transfer quantities may be
         * adjusted in order to transfer the entire quantity away from a Bundle.
         *
         * Calling
         *
         *     from.transfer(amount, to);
         *
         * is roughly equivalent to
         *
         *     from -= amount;
         *     to += amount;
         *
         * except for the error tolerance handling, and that the transfer is atomic (i.e. either
         * both transfers happen, or neither do).
         *
         * After the quantities have been transferred, any 0 values are removed by calling
         * clearZeros() on *this, `to`, and the returned bundle.
         *
         * The error handling is particularly useful to avoid problems with numerical imprecision
         * resulting in small negative quantities in Bundles.  In particular it specially does two
         * things:
         *
         * - If a transfer would result in a good in the source Bundle having a quantity with
         *   absolute value less than `epsilon` times the pre-transfer quantity, the transferred
         *   quantity of that good will instead be the total quantity in the source Bundle.
         * - Otherwise, if a transfer would results in a good in the destination Bundle having a
         *   quantity with absolute value less than `epsilon` times the pre-transfer quantity, the
         *   transferred quantity will be the amount required to reach exactly 0.  (Note that this
         *   case is only possible when the destination is a BundleSigned with a negative
         *   quantity, since the destination bundle is always the one being added to).
         *
         * \param amount the amount to transfer.  Goods with positive quantities are transferred
         * from the object into `to`; negative quantities are transferred frin `to` into the object.
         * \param to the Bundle (or BundleSigned) to transfer positive amounts to, and negative
         * amounts from.  `from` may be given as `amount` in order to transfer everything from
         * `from` to `to` (this is particularly useful if `from` is actually a Bundle rather than a
         * BundleSigned).
         * \param epsilon the threshold (relative to initial value) below which quantities in the
         * `amount` and `to` bundles will be truncated to 0.
         *
         * \returns the exact amount transferred, which may be slightly different from `amount`
         * because of numerical pricision handling.
         *
         * \throws Bundle::negativity_error if either the caller or `to` are actually Bundle objects
         * (rather than BundleSigned objects) with insufficient quantities to approximately
         * satisfy the transfer.
         *
         * \sa Bundle::hasApprox(const BundleSigned&, BundleSigned&, double)
         */
        BundleSigned transfer(const BundleSigned &amount, BundleSigned &to, double epsilon = default_transfer_epsilon);

        /** Transfers approximately the given amount from the caller object and returns it.  This is
         * like the above 3-argument transfer(const BundleSigned&, BundleSigned&, double)
         * except that the amount is not transferred into a target Bundle but simply returned.  Like
         * the 3-argument version, negative transfer amounts are added to the calling object and
         * will be negative in the returned object.
         *
         * Any zero-quantity goods will be removed from `*this` before returning.
         *
         * This method is also useful for adding or removing approximate amounts from a bundle by
         * simple ignoring the return value.  Thus the following:
         *
         *     bundle.transfer({ goodid, 3.0 });
         *     bundle.transfer({ goodid, -4.0 });
         *
         * is roughly the same as:
         *
         *     bundle[goodid] -= 3.0;
         *     bundle[goodid] += 4.0;
         *
         * except when the bundle initially contains values very close to (but not exactly equal to)
         * 3 or -1: in the former case, the first transfer() will remove slightly more or less
         * than 3 (and remove the good from the bundle entirely); the second transfer() adds
         * exactly 4 units of `goodid` to the bundle.  If the initial value is -1, the first call
         * subtracts exactly 3, and the second call ends up adding slightly more or less than 4 to
         * bring the quantity to exactly 0 (and then removes it).
         *
         * \param amount the amount to transfer from `*this`.
         * \param epsilon the threshold (relative to initial value) below which quantities in the
         * `amount` bundle will be truncated to 0.
         *
         * \returns the exact amount transferred out of *this, which may be slightly different from
         * `amount` because of numerical precision handling.
         *
         * \throws Bundle::negativity_error if either the caller is actually a Bundle object (rather
         * than BundleSigned) with insufficient quantities to approximately satisfy the transfer.
         *
         * \sa Bundle::hasApprox(const BundleSigned&, double)
         */
        BundleSigned transfer(const BundleSigned &amount, double epsilon = default_transfer_epsilon);

        /// transferApprox() is a deprecated name for transfer()
        template <typename... Args>
        [[deprecated("transferApprox() is deprecated; use transfer() instead")]]
            BundleSigned transferApprox(Args &&...args) {
            return transfer(std::forward<Args>(args)...);
        }

        /** Transfers all quantities held by the caller into the target bundle.  This uses
         * transfer() internally.  It is almost equivalent to `bundle.transfer(bundle, to)`, except
         * that `bundle` will always be cleared, while it is possible for `transfer()` to result
         * in some neglible quantities remaining in `bundle`.
         */
        BundleSigned transferTo(BundleSigned &to, double epsilon = default_transfer_epsilon);

        /// Adds two BundleSigned objects together and returns the result.
        BundleSigned operator + (const BundleSigned &b) const;
        /// Subtracts one BundleSigned from another and returns the result.
        BundleSigned operator - (const BundleSigned &b) const;
        /// Negates all quantities of a BundleSigned and returns the result.
        BundleSigned operator - () const;
        /// Scales all BundleSigned quantities by `m` and returns the result.
        BundleSigned operator * (double m) const;
        /// Scales all BundleSigned quantities by `1/d` and returns the result.
        BundleSigned operator / (double d) const;
        /// Scales all BundleSigned quantities by `m` and returns the result.
        friend BundleSigned operator * (double m, const BundleSigned &b);

        // Bundle <-> Bundle comparisons:
        
        /** Returns true if all quantities in the LHS bundle exceed those in the RHS bundle.  If one
         * of the two bundles is missing values contained in the other, those values are considered
         * to exist with numeric value 0 for the purpose of the comparison. */
        bool operator >  (const BundleSigned &b) const noexcept;
        /** Returns true if all quantities in the LHS bundle are at least as large as those in the
         * RHS bundle.  If one of the two bundles is missing values contained in the other, those
         * values are considered to exist with numeric value 0 for the purpose of the comparison. */
        bool operator >= (const BundleSigned &b) const noexcept;
        /** Returns true if all quantities in the LHS bundle are exactly equal to those in the RHS
         * bundle.  If one of the two bundles is missing values contained in the other, those
         * values are considered to exist with numeric value 0 for the purpose of the comparison. */
        bool operator == (const BundleSigned &b) const noexcept;
        /** Returns true if any quantities in the LHS bundle are not equal to those in the RHS
         * bundle.  If one of the two bundles is missing values contained in the other, those
         * values are considered to exist with numeric value 0 for the purpose of the comparison.
         * Note that `a != b` is equivalent to `!(a == b). */
        bool operator != (const BundleSigned &b) const noexcept;
        /** Returns true if all quantities in the RHS bundle exceed those in the LHS bundle.  If one
         * of the two bundles is missing values contained in the other, those values are considered
         * to exist with numeric value 0 for the purpose of the comparison. */
        bool operator <  (const BundleSigned &b) const noexcept;
        /** Returns true if all quantities in the RHS bundle are at least as large as those in the
         * LHS bundle.  If one of the two bundles is missing values contained in the other, those
         * values are considered to exist with numeric value 0 for the purpose of the comparison. */
        bool operator <= (const BundleSigned &b) const noexcept;

        // Bundle <-> constant comparisons:
        /** Returns true if every quantity that exists in this BundleSigned exceeds `q`.  Returns
         * true if the BundleSigned is empty. */
        bool operator >  (double q) const noexcept;
        /** Returns true if every quantity that exists in this BundleSigned equals or exceeds `q`.
         * Returns true if the BundleSigned is empty. */
        bool operator >= (double q) const noexcept;
        /** Returns true if every quantity that exists in this BundleSigned equals `q`.  Returns
         * true if the BundleSigned is empty. */
        bool operator == (double q) const noexcept;
        /** Returns true if any quantity that exists in this BundleSigned does not equal `q`.
         * Returns false if the BundleSigned is empty.  `a != 4` is equivalent to `!(a == 4)`. */
        bool operator != (double q) const noexcept;
        /** Returns true if every quantity that exists in this BundleSigned is less than `q`.
         * Returns true if the BundleSigned is empty. */
        bool operator <  (double q) const noexcept;
        /** Returns true if every quantity that exists in this BundleSigned is less than or equal
         * to `q`.  Returns true if the BundleSigned is empty. */
        bool operator <= (double q) const noexcept;

        // constant <-> Bundle comparisons:
        /// `q > bundle` is equivalent to `bundle < q`
        friend bool operator >  (double q, const BundleSigned &b) noexcept;
        /// `q >= bundle` is equivalent to `bundle <= q`
        friend bool operator >= (double q, const BundleSigned &b) noexcept;
        /// `q == bundle` is equivalent to `bundle == q`
        friend bool operator == (double q, const BundleSigned &b) noexcept;
        /// `q != bundle` is equivalent to `bundle != q`
        friend bool operator != (double q, const BundleSigned &b) noexcept;
        /// `q < bundle` is equivalent to `bundle > q`
        friend bool operator <  (double q, const BundleSigned &b) noexcept;
        /// `q <= bundle` is equivalent to `bundle >= q`
        friend bool operator <= (double q, const BundleSigned &b) noexcept;

        /** Begins a transaction for this bundle.  When a transaction is in progress, all Bundle
         * arithmetic is stored separately from the underlying Bundle results and can be reverted to
         * the point at which the transaction began by calling abortTransaction(), or committed to
         * the underlying Bundle by calling commitTransaction().
         *
         * Nested transactions are supported: when a nested transaction is completed, all changes
         * are propagated to the previous transaction instead of the base Bundle.  If a nested
         * transaction is aborted, Bundle quantities revert to the values present at the point the
         * nested transaction began.
         *
         * Code using transactions should catch exceptions that Bundle methods or operators might
         * throw, and be sure to abortTransaction() if such an exception occurs.
         *
         * \param encompassing if true (default to false), starts an "encompassing transaction": any
         * transactions that are started before this transaction ends will be absorbed into this one
         * rather than starting a nested transaction.  Note that this does not absolve nested
         * transactions from their responsibility to call abortTransaction() or commitTransaction():
         * those must still be called to distinguish where the encompassed transaction ends.
         */
        void beginTransaction(const bool encompassing = false) noexcept;

        /** Cancels a transaction previously started with beginTransaction(), restoring the Bundle's
         * quantities to the values before the transaction started.
         *
         * Does nothing (except noting that the transaction is over) if the current transaction was
         * started inside an encompassing transaction (or fake transaction started by
         * beginEncompassing()).
         *
         * \throws Bundle::no_transaction_exception if no transaction is currently active on this object.
         */
        void abortTransaction();

        /** Commits a tranaction previously started with beginTransaction().  Any changed quantities
         * will be propagated to the base Bundle quantities (or, in the case of nested transactions,
         * to the previous transaction quantities).
         *
         * Does nothing (except noting that the transaction is over) if the current transaction was
         * started inside an encompassing transaction (or fake transaction started by
         * beginEncompassing()).
         *
         * \throws Bundle::no_transaction_exception if no transaction is currently active on this object.
         */
        void commitTransaction();

        /** Starts a fake "transaction" that encompasses all transactions until endEncompassing() is
         * called to terminate the fake transaction.  This can be used on a bundle with no current
         * transaction to avoid any transactions for a section of code.
         *
         * Any transactions started after this call will be handled just like in
         * beginTransaction(true): transaction starting and ending will be a non-operation (but
         * tracked so that commitTransaction()/abortTransaction() calls can be matched up).
         *
         * This is used in cases where an exception would result in destruction of the object, and
         * so the usual transaction applied by various mutator methods are pointless and wasteful.
         * For example, Bundle subtraction creates a new Bundle, copying the left-hand-side Bundle,
         * then uses the `-=` operator to subtract the right-hand-side Bundle.  If the -= operator
         * throws an exception, the temporary Bundle is destroyed anyway, so transaction safety is
         * not needed.
         */
        void beginEncompassing() noexcept;

        /** Ends a fake encompassing transaction started by beginEncompassing().
         *
         * \throws Bundle::no_transaction_exception if there is still an outstanding (encompassed)
         * transaction that hasn't been committed or aborted, or if beginEncompassing() wasn't
         * called.
         */
        void endEncompassing();

        /** Exception thrown if attempting to abort or commit a transaction when no transaction has
         * been started.
         *
         * \sa beginTransaction()
         */
        class no_transaction_exception : public std::logic_error {
            public:
                /// no_transaction_exception constructor.  \param what an error message
                no_transaction_exception(const std::string &what) : std::logic_error(what) {}
                no_transaction_exception() = delete;
        };

        /** Overloaded so that a Bundle can be printed nicely with `std::cout << bundle`.
         *
         * Example outputs:
         *
         *     Bundle([4]=12, [6]=0)
         *     Bundle()
         *     BundleSigned([3]=-3.75, [2]=0, [1]=4.2e-24)
         *
         * The value in brackets is the eris::id_t of the good.
         */
        friend std::ostream& operator << (std::ostream &os, const BundleSigned &b);

    protected:
        /// Internal method used for bundle printing.
        void _print(std::ostream &os) const;

        /// Value proxy class that maps individual good quantity manipulation into set() calls.
        class valueproxy {
            private:
                BundleSigned &bundle_;
                const id_t gid_;
            public:
                valueproxy() = delete;
                /// Constructs a valueproxy from a BundleSigned and good id of the proxied value
                valueproxy(BundleSigned &bn, id_t gid) : bundle_(bn), gid_(gid) {}
                /// Assigns a new value to the proxied bundle quantity
                void operator=(double q) { bundle_.set(gid_, q); }
                /// Adds a value to the current proxied bundle quantity
                void operator+=(double q) { bundle_.set(gid_, *this + q); }
                /// Subtracts a value from the current proxied bundle quantity
                void operator-=(double q) { bundle_.set(gid_, *this - q); }
                /// Scales the value of the current proxied bundle quantity
                void operator*=(double q) { bundle_.set(gid_, *this * q); }
                /// Scales the value of the current proxied bundle quantity
                void operator/=(double q) { bundle_.set(gid_, *this / q); }
                /// Const access to the double quantity underlying this proxy
                operator const double&() const { return const_cast<const BundleSigned&>(bundle_)[gid_]; }
        };

    private:
        // The stack of quantity maps; the front of q_stack_ is the currently visible quantities;
        // remainder items are the pre-transaction values.
        std::forward_list<std::unordered_map<id_t, double>> q_stack_ = {std::unordered_map<id_t, double>()};

        // If non-empty, we're inside an encompassing transaction or fake transaction.  The value at
        // the beginning of the list tells us whether it's a encompassing transaction (started by
        // beginTransaction()), if true, or a encompassing non-transaction (started by
        // beginEncompassing()), if false.
        std::forward_list<bool> encompassed_;

        // Zero; returned as const double& when const-accessing a good that doesn't exist
        static constexpr double zero_ = 0.0;
};

/// BundleNegative is a deprecated alias for BundleSigned
using BundleNegative = BundleSigned;

class Bundle final : public BundleSigned {
    public:
        /// Creates a new, empty Bundle.
        Bundle();
        /// Creates a new Bundle containing a single good of the given quantity.
        Bundle(MemberID g, double q);
        /// Creates a new Bundle from an initializer list of goods and quantities.
        Bundle(const std::initializer_list<std::pair<id_t, double>> &init);
        /** Creates a new Bundle by copying quantities from a BundleSigned.  If the other Bundle is
         * currently in a transaction, only the current values are copied; the transactions and
         * pre-transactions values are not.
         *
         * \throws Bundle::negativity_error if the given BundleSigned has negative quantities.
         */
        Bundle(const BundleSigned &b);

        /// Copy constructor.  Only the current values, not the transaction state, is copied.
        Bundle(const Bundle &b);

        /** Move constructor.  Unlike the copy constructor, this preserves the transaction state. */
        Bundle(Bundle &&b) = default;

        /** Sets the quantitity associated with good `gid` to `quantity`.
         *
         * \throws Bundle::negativity_error if quantity is negative.
         */
        void set(MemberID gid, double quantity) override;

        /** Assigns a bundle to this bundle.  The transaction state of the current bundle is
         * maintained, while only the currently-visible values of the given bundle are copied
         * into the current bundle (or bundle transaction, if one is in progress).
         */
        Bundle& operator = (const Bundle &b);

        /** Adds a bundle (or signed bundle) to this bundle.  This is exactly the same as the
         * BundleSigned += operator, but returns the object cast as a Bundle& rather than
         * BundleSigned&.
         *
         * The addition is carried out in a transaction: that is, if it fails because some
         * quantities would become negative, no quantities will have been changed.
         */
        Bundle& operator += (const BundleSigned &b);

        /** Subtracts a bundle (or signed bundle) from this bundle.  This simply calls the
         * BundleSigned += operator, but returns the object cast as a Bundle& instead of a
         * BundleSigned&.
         *
         * The subtraction is carried out in a transaction: that is, if it fails because some
         * quantities would become negative, no quantities will have been changed.
         */
        Bundle& operator -= (const BundleSigned &b);

        /** Scales a bundle by `m`.
         *
         * \throws Bundle::negativity_error if `m` is negative.
         */
        Bundle& operator *= (double m);
        /** Scales a bundle by `1/d`.
         *
         * \throws Bundle::negativity_error if `d` is negative.
         */
        Bundle& operator /= (double d);

        // Explicitly inherit the BundleSigned versions of these (because otherwise the operator +
        // below would hide them)
        using BundleSigned::operator +;
        using BundleSigned::operator -;
        /** Adding two Bundle objects returns a Bundle object.  Note that this operator is only
         * invoked when the visible type of both Bundles is Bundle.  In other words, in the
         * following code `c` will be a Bundle and `d` will be a BundleSigned:
         *
         *     Bundle a = ...;
         *     Bundle b = ...;
         *     auto c = a + b;
         *     auto d = a + (BundleSigned) b;
         */
        Bundle operator + (const Bundle &b) const;
        /** Subtracting two Bundle objects returns a Bundle object.  Note that this can throw an
         * exception if the quantity any good in b is greater than in a.  If you want to allow for
         * the quantity to be negative, you should cast one of the objects to a BundleSigned or
         * use unary minus (which returns a BundleSigned), such as one of these:
         *
         *     Bundle a({{ 1, 2 }});
         *     Bundle b({{ 1, 3 }});
         *
         *     auto pos1 = b - a;                  // Bundle([1]=1)
         *     auto neg1 = (BundleSigned) a - b; // BundleSigned([1]=-1)
         *     auto neg2 = -b + a;                 // Same
         *     auto neg3 = a - b; // Oops: throws eris::Bundle::negativity_error
         *
         * The `neg1` calculation above is slightly more efficient than neg2 as it doesn't need to
         * create the intermediate `-b` BundleSigned object.
         */
        Bundle operator - (const Bundle &b) const;
        /** Multiplying a Bundle by a constant returns a Bundle with quantities scaled by the
         * constant.
         *
         * \throws Bundle::negativity_error if `m < 0`.  If you need a negative, first cast the Bundle
         * as a BundleSigned, such as: `auto neg = ((BundleSigned) b) * -3;`
         */
        Bundle operator * (double m) const;
        /** Scales all BundleSigned quantities by `m` and returns the result.
         *
         * \throws Bundle::negativity_error if `m < 0`.  If you need a negative, first cast the Bundle
         * as a BundleSigned, such as: `auto neg = -3 * (BundleSigned) b;`
         */
        friend Bundle operator * (double m, const Bundle &a);
        /** Dividing a Bundle by a constant returns a Bundle with quantities scaled by the inverse
         * of the constant.
         *
         * \throws Bundle::negativity_error if m < 0
         */
        Bundle operator / (double d) const;

        /** Performs "bundle coverage," returning the multiples of the provided bundle needed to
         * cover the called-upon Bundle.  Coverage of a Bundle (but *not* BundleSigned) by another
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
         * \see reduce(BundleSigned&, BundleSigned&)
         */
        static Bundle common(const BundleSigned &a, const BundleSigned &b) noexcept;
        /** This is like common(), but it also subtracts the common Bundle from both bundles before
         * returning it.
         */
        static Bundle reduce(BundleSigned &a, BundleSigned &b);

        /** Returns true if the called-upon bundle has approximately enough of each
         * positive-quantity good in `amount`, and the bundle `to` has approximately enough of each
         * negative-quantity good in `amount`.
         *
         *     a.hasApprox(transfer, b)
         *
         * is notionally equivalent to:
         *
         *     (a >= transfer.positive() && b >= transfer.negative())
         *
         * except that it allows slight numerical imprecision when the compared amounts are very
         * similar.
         *
         * If this method returns true, it is a guarantee that transfer() called with the same
         * arguments will succeed (i.e. without resulting in a Bundle::negativity_error exception).
         */
        bool hasApprox(const BundleSigned &amount, const Bundle &to, double epsilon = default_transfer_epsilon) const;

        /** Returns true if the called-upon bundle has approximately enough of each
         * positive-quantity good in `amount` to complete a transfer via transfer().  Negative
         * quantites in `amount` are ignored.
         *
         *     a.hasApprox(bundle)
         *
         * is notionally equivalent to:
         *
         *     (a >= bundle.positive())
         *
         * except that it allows for numerical error for goods with very similar quantities.
         *
         * If this method returns true, it is a guarantee that transfer() called with the same
         * arguments will succeed (i.e. without resulting in a Bundle::negativity_error exception).
         */
        bool hasApprox(const BundleSigned &amount, double epsilon = default_transfer_epsilon) const;

        /** Overloaded so that a Bundle can be printed nicely with `std::cout << bundle`.
         *
         * Example outputs:
         *
         *     Bundle([4]=12, [6]=0)
         *     Bundle()
         *     BundleSigned([3]=-3.75, [2]=0, [1]=4.2e-24)
         *
         * The value in brackets is the eris::id_t of the good.
         */
        friend std::ostream& operator << (std::ostream &os, const Bundle& b);

        /** Exception class for attempting to do some operation that would resulting in a negative
         * quantity being set in a Bundle.
         */
        class negativity_error : public std::range_error {
            public:
                /** Constructor for a generic negativity exception for a negative quantity.
                 *
                 * \param good the id of the good that was assigned a negative value
                 * \param value the negative value that was assigned
                 */
                negativity_error(id_t good, double value) :
                    std::range_error("good[" + std::to_string(good) + "] assigned illegal negative value "
                            + std::to_string(value) + " in Bundle."), good(good), value(value) {}
                /** Constructor for a negativity exception for a negative quantity with a given
                 * error message.
                 *
                 * \param what_arg the error message
                 * \param good the id of the good that was assigned a negative value
                 * \param value the negative value that was assigned
                 */
                negativity_error(const std::string& what_arg, id_t good, double value) :
                    std::range_error(what_arg), good(good), value(value) {}
                /// The id of the good that caused the error
                const id_t good;
                /// The illegal value that caused the error
                const double value;
        };
};

}
