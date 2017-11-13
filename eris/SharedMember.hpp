#pragma once
#include <eris/types.hpp>
#include <cstddef>
#include <memory>
#include <typeinfo>
#include <functional>
#include <type_traits>
#include <ostream>

namespace eris {

/** Wrapper around std::shared_ptr<T> that adds automatic T cast conversion and automatic
 * up/downcasting.  Since Member references must be stored both by calling code, the Simulation
 * object, and potentially in other classes, a Simulation's members are stored in std::shared_ptr
 * objects.  This wrapper allows transparent access to the underlying Member, including automatic
 * Member and member method access via ->.  It also supports automatic cast conversion between
 * sub/superclasses: e.g. a SharedMember<Good::Continuous> instance can be implicitly converted to a
 * SharedMember<Member>, and the same SharedMember<Member> variable can be implicitly converted to a
 * SharedMember<Good>.
 *
 * A less-than comparison operator is provided so that SharedMembers can be stored in a sorted
 * container.
 */
template<class T>
class SharedMember final {
public:
    /** Default constructor; creates a SharedMember around a null shared_ptr that refers to no
     * object.
     */
    SharedMember() {}

    /** The type T that this SharedMember wraps */
    using member_type = T;

    /** Bool operator; returns true if the SharedMember actually references a Member. */
    explicit operator bool() const { return (bool) ptr_; }

    /** Constructs a SharedMember from a copy of the given shared_ptr.  This also allows
     * implicit conversion from a shared_ptr to a SharedMember. */
    SharedMember(const std::shared_ptr<T> &ptr) : ptr_{ptr} {}

    /** Constructs a SharedMember by moving the given shared_ptr rvalue. */
    SharedMember(std::shared_ptr<T> &&ptr) : ptr_{std::move(ptr)} {}

    /** Constructs a SharedMember from a pointer.  The management of the pointer is taken over
     * by the new SharedMember<T>'s internal shared_ptr.
     */
    explicit SharedMember(T *ptr) : ptr_{ptr} {}

    /** Implicit conversion to T& */
    operator T& () const { return *ptr_; }
    /** Dereferencing gives you the underlying T */
    T& operator * () const { return *ptr_; }
    /** Dereferencing member access works on the underlying T */
    T* operator -> () const { return ptr_.get(); }
    /// Returns the stored pointer
    T* get() const { return ptr_.get(); }

    /// Implicit conversion to a shared_ptr that shares this object's pointer
    operator std::shared_ptr<T> () const noexcept { return ptr_; }

    /** Equality comparison.  Two SharedMember<T> objects are considered equal if and only if
     * they are both set and both have equal id() values, or are both unset.  The two objects
     * are not required to have the same `T` (so that a SharedMember<Member> will == a
     * SharedMember<Good> if both refer to the same object).
     */
    template <class O>
    bool operator == (const SharedMember<O> &other) const noexcept {
        if (!ptr_) return !other.ptr_;
        if (!other.ptr_) return false;
        return ptr_->id() == other.ptr_->id();
    }

    /// Inequality comparison.  This simply returns the negation of the == operator.
    template <class O> bool operator != (const SharedMember<O> &other) const noexcept {
        return !(*this == other);
    }

    /** Less-than comparison operator.  This method is guaranteed to provide a unique ordering
     * of SharedMembers that have a referenced Member.  This is currently done by comparing id()
     * values (and using 0 for null SharedMembers), but that behaviour could change.
     */
    template <class O>
    bool operator < (const SharedMember<O> &other) const noexcept {
        return (*this ? ptr_->id() : 0) < (other ? other->id() : 0);
    }

    /** const access to the underlying shared_ptr */
    const std::shared_ptr<T>& ptr() const & noexcept { return ptr_; }

    /** In rvalue context the current shared pointer can be obtained as an rvalue. */
    std::shared_ptr<T>&& ptr() && noexcept { return std::move(ptr_); }

    /** Resets the underlying shared_ptr */
    void reset() { ptr_.reset(); }

    /** Constructing a new SharedMember<A> using an existing SharedMember<F> recasts the F
     * pointer to an A pointer.  This constructor also allows you to do things like:
     * \code
     *     SharedMember<B> actual = ...
     *     SharedMember<A> a1 = actual;
     *     SharedMember<B> b1 = a1;
     * \endcode
     * Or, alternatively,
     * \code
     *     SharedMember<A> a2(actual);
     *     SharedMember<B> b2(a2);
     * \endcode
     *
     * This template specification only participates when upcasting from a derived to a base
     * class.
     */
    template<class F, std::enable_if_t<std::is_base_of<T, F>::value && !std::is_same<T, F>::value, int> = 0>
    SharedMember(const SharedMember<F> &from)
        : ptr_{std::static_pointer_cast<T,F>(from.ptr())}
    {}

    /** Same as above, but for downcasting (i.e. when going from SharedMember<Base> to
     * SharedMember<Derived>). This needs to use a dynamic cast with run-time type checking
     * because it isn't guaranteed that a particular SharedMember<Base> is actually a
     * SharedMember<Derived>.
     *
     * This template specification only participates when downcasting from a base to a derived
     * class.
     *
     * \throws std::bad_cast if `*from` is not an instance of `T`.
     */
    template<class F, std::enable_if_t<std::is_base_of<F, T>::value && !std::is_same<T, F>::value, int> = 0>
    SharedMember(const SharedMember<F> &from)
        : ptr_{std::dynamic_pointer_cast<T,F>(from.ptr())} {
        // Raise an exception if the ptr above gave back a null shared pointer: that means the
        // cast attempted to cast to a derived class, when the actual object is only a base
        // class instance or some other derived instance.
        if (from.ptr() && !ptr_) throw std::bad_cast();
    }

    /// Default copy constructor.
    SharedMember(const SharedMember &) = default;

    /// Default move constructor.  The underlying shared pointer is moved
    SharedMember(SharedMember &&) = default;

    /// Default copy assignment
    SharedMember& operator=(const SharedMember &) = default;

    /// Default move assignment
    SharedMember& operator=(SharedMember &&) = default;

    /// Output support; outputs '<null member>' for an empty reference, otherwise forwards to
    /// underlying T's `operator <<`.
    friend std::ostream& operator << (std::ostream &os, const SharedMember &s) {
        return s.ptr_ ? os << *s : os << "<null member>";
    }

private:
    std::shared_ptr<T> ptr_;

    template <typename O> friend class SharedMember;
};

/** Weak-reference version of SharedMember<T>.  This is internally a wrapper around a
 * `std::weak_ptr<T>` that weakly references a member.  It is implicitly convertible from a
 * `SharedMember<T>`, and implicitly convertible to a `SharedMember<U>` for any `U` for which
 * `SharedMember<T>` is implicitly convertible.  It also can be used in boolean context (in the same
 * way as SharedMember, i.e. to determine if there is actually a reference).  It is not otherwise
 * directly usable: any member access must operate by first converting into a SharedMember<T>.
 */
template <class T>
class WeakMember final {
    template <typename U> constexpr static bool convertible_from = std::is_convertible<SharedMember<U>, SharedMember<T>>::value;
public:
    /// Default constructor; the weak pointer doesn't have a target.
    WeakMember() = default;

    /// Copy constructor
    WeakMember(const WeakMember &) = default;

    /// Move constructor
    WeakMember(WeakMember &&) = default;

    /// Constructs a weak member from a shared member; allows implicit conversion
    WeakMember(const SharedMember<T> &member) :
        ptr_{member.ptr()} {};

    /// Implicit conversion from this weak member to a shared member of the same type
    operator SharedMember<T>() const { return SharedMember<T>(ptr_.lock()); }

    /// Same as implicit conversion to a SharedMember<T>, but easier in some contexts.
    SharedMember<T> lock() const { return SharedMember<T>(ptr_.lock()); }

    /// Implicit conversion from this WeakMember<T> to a related SharedMember<U>; this operator only
    /// participates when `SharedMember<T>` is convertible to `SharedMember<U>`.  Also note that
    /// this conversion may fail (see SharedMember implicit conversion for details).
    template <typename U, typename = std::enable_if_t<convertible_from<U>>>
    operator SharedMember<U>() const { return SharedMember<U>(*this); }

    /// Default copy assignment
    WeakMember& operator=(const WeakMember &) = default;

    /// Default move assignment
    WeakMember& operator=(WeakMember &&) = default;

    /// Copy assignment from a SharedMember<T>; replaces current weak pointer with new weak pointer
    /// referencing the given member.
    WeakMember& operator=(const SharedMember<T> &m) { ptr_ = m.ptr(); return *this; }

    /// Copy assignment from a SharedMember<T>, rvalue version.
    WeakMember& operator=(SharedMember<T> &&m) { ptr_ = std::move(m).ptr(); return *this; }

    /// Copy assignment from a SharedMember<U>; only participates if convertible to SharedMember<T>.
    /// Also note that this can throw at runtime whenever the SharedMember conversion throws.
    template <typename U, typename = std::enable_if_t<convertible_from<U>>>
    WeakMember &operator=(const SharedMember<U> &m) { *this = (SharedMember<T>) m; return *this; }

    /// boolean context: returns true if there is a referenced member.  Note that if not used
    /// carefully this can have a race condition: the referenced member could be set (or disappear)
    /// between the boolean conversion and the subsequent operation.
    operator bool() { return !ptr_.expired(); }

    /// Output support; equivalent to `os << w.lock()`.
    friend std::ostream& operator << (std::ostream &os, const WeakMember &w) { return os << w.lock(); }

private:
    std::weak_ptr<T> ptr_;
};

}

namespace std {
/// std::hash implementation for a SharedMember<T>.  Returns the hash for the underlying shared_ptr.
template <class T>
struct hash<eris::SharedMember<T>> {
public:
    /// Returns the hash for the SharedMember's underlying ptr
    size_t operator()(const eris::SharedMember<T> &m) const { return std::hash<std::shared_ptr<T>>()(m.ptr()); }
};

}
