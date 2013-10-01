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
        operator T& () const { return *ptr; }
        /** Using as an eris_id_t gives you the object's id */
        operator eris_id_t () const { return ptr->id(); }
        /** Dereferencing gives you the underlying T */
        T& operator * () const { return *ptr; }
        /** Dereferencing member access works on the underlying T */
        T* operator -> () const { return ptr.get(); }

        /** The underlying shared_ptr */
        const std::shared_ptr<T> ptr;

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
        template<class F> SharedMember(const SharedMember<F> &from) : ptr(std::dynamic_pointer_cast<T,F>(from.ptr)) {
            // Raise an exception if the ptr above gave back a null shared pointer: that means the
            // cast attempted to cast to a derived class, when the actual object is only a base
            // class instance.
            if (!ptr) throw std::bad_cast();
        }

        SharedMember<T>& operator=(const SharedMember<T> &t) = delete;

    private:
        SharedMember(std::shared_ptr<T> ptr) : ptr(ptr) {}

        friend class Simulation;
        friend class Member;
};

}
