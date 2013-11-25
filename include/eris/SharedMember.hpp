#pragma once
#include <eris/types.hpp>
#include <memory>
#include <stdexcept>

namespace eris {

class Member;

/** Wrapper around std::shared_ptr<T> that adds automatic T and eris_id_t cast conversion.  Since
 * Member references must be stored both by calling code, the Simulation object, and potentially in
 * other classes, a Simulation's members are stored in std::shared_ptr objects.  This wrapper allows
 * transparent access to the underlying Member, including automatic Member and eris_id_t converstion
 * and member method access via ->.  It also supports automatic cast conversion between
 * sub/superclasses: e.g. a SharedMember<Good::Continuous> instance can be cast as a
 * SharedMember<Member>, and the same SharedMember<Member> variable can be cast as a
 * SharedMember<Good>.
 *
 * A less-than comparison operator is provided so that SharedMembers can be stored in an ordered
 * container.
 *
 * Note that SharedMember instances *cannot* be constructed directly (except when copying from
 * another compatible SharedMember instance), but are created via the various add* and clone*
 * methods of Simulation.
 */
template<class T>
class SharedMember final {
    public:
        /** Default constructor; creates a SharedMember around a null shared_ptr. This shouldn't be
         * used, but is needed so that, for example, a SharedMember<T> can be the type of values in
         * a std::map (and similar classes).
         */
        SharedMember() {}
        /** Using as a T gives you the underlying T object */
        operator T& () const { return *ptr_; }
        /** Using as an eris_id_t gives you the object's id */
        operator eris_id_t () const { return ptr_->id(); }
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
            return (ptr_ ? ptr_->id() : 0) < (other.ptr_ ? other.ptr_->id() : 0); }

        /** Access to the underlying shared_ptr */
        const std::shared_ptr<T> ptr() const { return ptr_; }

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
         * Assuming that A is a base class of B.  Attempting to cast to an unsupported class (for
         * example, attempting to cast a SharedMember<Good> (whether in a SharedMember<Good> or
         * SharedMember<Member> variable) as a SharedMember<Agent>) will throw a bad_cast exception
         * from std::dynamic_cast.  Note that cases where dynamic_cast returns a null pointer
         * (attempting to cast a base instance to a derived class) will also throw a bad_cast
         * exception, unlike std::dynamic_cast.
         *
         * Note that when A and F are the same thing, this is a copy constructor that simply copies
         * the underlying std::shared_ptr.
         */
        template<class F> SharedMember(const SharedMember<F> &from) : ptr_(std::dynamic_pointer_cast<T,F>(from.ptr())) {
            // Raise an exception if the ptr above gave back a null shared pointer: that means the
            // cast attempted to cast to a derived class, when the actual object is only a base
            // class instance.
            if (!ptr_) throw std::bad_cast();
        }

        /** Copy assignment operator.  This is only permitted when the current shared pointer is
         * null; it is sometimes needed by standard library components (such as when copying
         * containers).  Since a shared pointer can never be changed once set, this operator can
         * only be called once.
         *
         * \throws std::runtime_error if the object already has a shared pointer.
         */
        SharedMember<T>& operator=(const SharedMember<T> &t) {
            if (ptr_) throw std::runtime_error("Cannot reassign SharedMember<T> via assignment operator");
            ptr_ = t.ptr_;
            return *this;
        }


    private:
        SharedMember(std::shared_ptr<T> ptr) : ptr_(ptr) {}

        std::shared_ptr<T> ptr_;

        friend class Simulation;
        friend class Member;
};

}
