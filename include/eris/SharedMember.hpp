#pragma once
#include <eris/types.hpp>
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include <functional>

namespace eris {

/** Wrapper around std::shared_ptr<T> that adds automatic T and eris_id_t cast conversion.  Since
 * Member references must be stored both by calling code, the Simulation object, and potentially in
 * other classes, a Simulation's members are stored in std::shared_ptr objects.  This wrapper allows
 * transparent access to the underlying Member, including automatic Member and eris_id_t converstion
 * and member method access via ->.  It also supports automatic cast conversion between
 * sub/superclasses: e.g. a SharedMember<Good::Continuous> instance can be cast as a
 * SharedMember<Member>, and the same SharedMember<Member> variable can be cast as a
 * SharedMember<Good>.
 *
 * A less-than comparison operator is provided so that SharedMembers can be stored in a sorted
 * container.
 *
 * Note that SharedMember instances *cannot* be constructed directly (except when copying from
 * another compatible SharedMember instance), but are created via the Simulation::create method.
 */
template<class T>
class SharedMember final {
    public:
        /** Default constructor; creates a SharedMember around a null shared_ptr that refers to no
         * object.
         */
        SharedMember() : orig_type{&typeid(T)} {}

        /** Using as a T gives you the underlying T object */
        operator T& () const { return *ptr_; }
        /** Using as an eris_id_t gives you the object's id.  This returns 0 if the pointer is
         * a null pointer, or if the object does not belong to a simulation.  (To distinguish
         * between the two, call .ptr() in a boolean context).
         *
         * This method also doubles as a boolean operator.
         */
        operator eris_id_t () const { return ptr_ ? ptr_->id() : 0; }
        /** Dereferencing gives you the underlying T */
        T& operator * () const { return *ptr_; }
        /** Dereferencing member access works on the underlying T */
        T* operator -> () const { return ptr_.get(); }

        /** Less-than comparison operator.  This currently compares eris_id_t values, but that could
         * be subject to change.  If either SharedMember doesn't have an actual member, a value of 0
         * is used.  0 is also used for Members that are not part of a simulation.
         *
         * eris_id_t values are unique within a simulation, but not across simulations: attempting
         * to store SharedMember<T> of different simulations in a container will not work.
         */
        template <class O>
        bool operator < (const SharedMember<O> &other) {
            return (eris_id_t) *this < (eris_id_t) other;
        }

        /** Access to the underlying shared_ptr */
        const std::shared_ptr<T>& ptr() const { return ptr_; }

        /** The type T that this SharedMember wraps */
        typedef T member_type;

        /** The original type of the pointed-at object.  For example, if the pointed-at object was
         * created as a R, and R inherits from S and S inherits from T, then any SharedMember<S> or
         * SharedMember<T> created from casting this SharedMember<R> will have .orig_type =
         * &typeid(R).
         *
         * This is used during copy-assignment to allow copy-assignment only when the original types
         * of the pointed at objects are identical.
         */
        const std::type_info *orig_type;

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
         * Assuming that A is a base class of B.  Attempting to cast to an unsupported class (for
         * example, attempting to cast a SharedMember<Good> (whether in a SharedMember<Good> or
         * SharedMember<Member> variable) as a SharedMember<Agent>) will throw a bad_cast exception
         * from std::dynamic_cast.  Note that cases where dynamic_cast returns a null pointer
         * (attempting to cast a base instance to a derived class) will also throw a bad_cast
         * exception, unlike std::dynamic_cast.
         */
        template<class F>
        SharedMember(const SharedMember<F> &from,
                typename std::enable_if<std::is_base_of<T, F>::value and not std::is_same<T, F>::value>::type* = 0)
            : orig_type{from.orig_type}, ptr_{std::static_pointer_cast<T,F>(from.ptr())}
        {}

        /** Same as above, but for downcasting (i.e. when going from SharedMember<Base> to
         * SharedMember<Derived>). This needs to use a dynamic cast with run-time type checking
         * because it isn't guaranteed that a particular SharedMember<Base> is actually a
         * SharedMember<Derived>. */
        template<class F>
        SharedMember(const SharedMember<F> &from,
                typename std::enable_if<std::is_base_of<F, T>::value and not std::is_same<T, F>::value>::type* = 0)
            : orig_type{from.orig_type}, ptr_{std::dynamic_pointer_cast<T,F>(from.ptr())} {
            // Raise an exception if the ptr above gave back a null shared pointer: that means the
            // cast attempted to cast to a derived class, when the actual object is only a base
            // class instance.
            if (from.ptr() and not ptr_) throw std::bad_cast();
        }

        /** Copy constructor. */
        SharedMember(const SharedMember<T> &from) : orig_type{from.orig_type}, ptr_{from.ptr_}
        {}

        /** Copy assignment operator.  This is permitted in only two circumstances: first, when the
         * current shared pointer is null; this is sometimes needed by standard library components
         * (such as when copying containers), which need to create a SharedMember<T> with no
         * arguments then copy-assign into it.
         *
         * The second permitted situation is when the two pointed-at objects are actually of the
         * same time, regardless of whatever T they are currently cast into.  For example:
         *
         *     SharedMember<AssetAgent> aa1 = sim->create<AssetAgent>();
         *     SharedMember<AssetAgent> aa2 = sim->create<AssetAgent>();
         *     SharedMember<Positional<AssetAgent>> pa3 = sim->create<Positional<AssetAgent>>();
         *
         *     // aa1 and aa2 can be cast to SharedMember<B> for any base class of AssetAgent
         *     // pa3 can be cast to the same SharedMember<B> and also a SharedMember<AssetAgent>
         *     // (because AssetAgent is a base class of Positional<AssetAgent>)
         *
         *     SharedMember<Agent> agent1 = aa1; // Upcasts SharedMember<AssetAgent> to SharedMember<Agent>
         *     SharedMember<Member> memb2 = aa2; // Upcasts SharedMember<AssetAgent> to SharedMember<Member>
         *     SharedMember<Agent> agent3 = pa3; // Upcasts to SharedMember<Agent>
         *
         *     memb2 = aa1; // Allowed: both "1" and "2" are really AssetAgent objects
         *     agent3 = aa2; // Throws runtime_error: "2" and "3" are not the same original type
         *
         * \throws std::runtime_error if the object already has a shared pointer and that shared
         * pointer is not of the same original type as the copied-in object.
         */
        SharedMember<T>& operator=(const SharedMember<T> &t) {
            if (not ptr_) {
                // null SharedMember<T> that we're copying into
                orig_type = t.orig_type;
            }
            else if (*t.orig_type != *orig_type) {
                throw std::runtime_error("Cannot reassign SharedMember<T> via assignment operator when original types differ");
            }
            ptr_ = t.ptr_;
            return *this;
        }

    private:
        SharedMember(const std::shared_ptr<T> &ptr) : orig_type{&typeid(T)}, ptr_{ptr} {}
        SharedMember(std::shared_ptr<T> &&ptr) : orig_type{&typeid(T)}, ptr_{std::move(ptr)} {}

        std::shared_ptr<T> ptr_;

        friend class Simulation;
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
