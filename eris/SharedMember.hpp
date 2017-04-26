#pragma once
#include <eris/types.hpp>
#include <cstddef>
#include <memory>
#include <typeinfo>
#include <functional>
#include <type_traits>

namespace eris {

/** Wrapper around std::shared_ptr<T> that adds automatic T and eris_id_t cast conversion.  Since
 * Member references must be stored both by calling code, the Simulation object, and potentially in
 * other classes, a Simulation's members are stored in std::shared_ptr objects.  This wrapper allows
 * transparent access to the underlying Member, including automatic Member and eris_id_t conversion
 * and member method access via ->.  It also supports automatic cast conversion between
 * sub/superclasses: e.g. a SharedMember<Good::Continuous> instance can be cast as a
 * SharedMember<Member>, and the same SharedMember<Member> variable can be cast as a
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

        /** Constructs a SharedMember from a copy of the given shared_ptr.  This also allows
         * implicit conversion from a shared_ptr to a SharedMember. */
        SharedMember(const std::shared_ptr<T> &ptr) : ptr_{ptr} {}

        /** Constructs a SharedMember by moving the given shared_ptr rvalue. */
        SharedMember(std::shared_ptr<T> &&ptr) : ptr_{std::move(ptr)} {}

        /** Constructs a SharedMember from a pointer.  The management of the pointer is taken over
         * by the new SharedMember<T>'s internal shared_ptr.
         */
        explicit SharedMember(T *ptr) : ptr_{ptr} {}

        /** Using as a T gives you the underlying T object */
        operator T& () const { return *ptr_; }
        /** Using as an eris_id_t gives you the object's id.  This returns 0 if the pointer is
         * a null pointer, or if the object does not belong to a simulation.  (To distinguish
         * between the two, call .ptr() in a boolean context).
         *
         * This method also doubles as a boolean operator.
         */
        operator eris_id_t () const noexcept { return ptr_ ? ptr_->id() : 0; }
        /** Dereferencing gives you the underlying T */
        T& operator * () const { return *ptr_; }
        /** Dereferencing member access works on the underlying T */
        T* operator -> () const { return ptr_.get(); }
        /// Returns the stored pointer
        T* get() const { return ptr_.get(); }

        /// Conversion operator: returns a shared_ptr that shares this object's pointer
        operator std::shared_ptr<T> () const noexcept { return ptr_; }

        /** Equality comparison.  Two SharedMember objects are considered equal if and only if they
         * both have positive and equal eris_id_t values; if either object is a null pointer, or
         * either object has an eris_id_t of 0, the two are not considered equal (even if both are
         * null, or both have eris_id_t of 0).
         */
        template <class O>
        bool operator == (const SharedMember<O> &other) noexcept {
            if (not ptr_ or not other.ptr_) return false;
            eris_id_t myid = ptr_->id();
            if (myid == 0) return false;
            eris_id_t otherid = other.ptr_->id();
            if (otherid == 0) return false;
            return myid == otherid;
        }

        /// Inequality comparison.  This simply returns the negation of the == operator.
        template <class O> bool operator != (const SharedMember<O> &other) noexcept {
            return not(*this == other);
        }

        /** Less-than comparison operator.  This method is guaranteed to provide a unique ordering
         * of SharedMembers that belong to the same simulation.  This is currently done by sorting
         * by eris_id_t value (and using 0 for null SharedMembers and Members that are not part of a
         * simulation), but that behaviour could change.
         */
        template <class O>
        bool operator < (const SharedMember<O> &other) noexcept {
            return (eris_id_t) *this < (eris_id_t) other;
        }

        /** const access to the underlying shared_ptr */
        const std::shared_ptr<T>& ptr() const noexcept { return ptr_; }

        /** The type T that this SharedMember wraps */
        typedef T member_type;

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
        template<class F>
        SharedMember(const SharedMember<F> &from,
                typename std::enable_if<std::is_base_of<T, F>::value and not std::is_same<T, F>::value>::type* = 0)
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
        template<class F>
        SharedMember(const SharedMember<F> &from,
                typename std::enable_if<std::is_base_of<F, T>::value and not std::is_same<T, F>::value>::type* = 0)
            : ptr_{std::dynamic_pointer_cast<T,F>(from.ptr())} {
            // Raise an exception if the ptr above gave back a null shared pointer: that means the
            // cast attempted to cast to a derived class, when the actual object is only a base
            // class instance.
            if (from.ptr() and not ptr_) throw std::bad_cast();
        }

        /// Default copy constructor.
        SharedMember(const SharedMember<T> &) = default;

        /// Default move constructor.  The underlying shared pointer is moved
        SharedMember(SharedMember<T> &&) = default;

        /// Default copy assignment
        SharedMember<T>& operator=(const SharedMember<T> &) = default;

        /// Default move assignment
        SharedMember<T>& operator=(SharedMember<T> &&) = default;

    private:
        std::shared_ptr<T> ptr_;

        template <typename O> friend class SharedMember;
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
