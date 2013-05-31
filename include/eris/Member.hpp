#pragma once
#include <eris/types.hpp>
#include <memory>

namespace eris {

class Simulation;
class Optimizer;

/** Base class for "members" of a simulation: goods, agents, and markets.  This class provides an
 * id, a weak reference to the owning simulation object, and a < operator comparing ids (so that
 * ordering works).
 */
class Member {
    public:
        virtual ~Member() = default;
        /** Returns the eris_id_t ID of this member.  Returns 0 if this Member instance has not yet
         * been added to a Simulation.
         */
        eris_id_t id() const { return _id; }
        /** A Member object can be used anywhere an eris_id_t value is called for and will evaluate
         * to the Member's ID.
         */
        operator eris_id_t() const { return _id; }
    protected:
        /** Stores a weak pointer to the simulation this Member belongs to. */
        std::weak_ptr<eris::Simulation> simulation;
        friend eris::Simulation;
        friend eris::Optimizer;
    private:
        eris_id_t _id = 0;
        friend eris::Simulation;
};

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
 * another compatible SharedMember instance), but are created via the addGood, addAgent, and
 * addMarket methods of Simulation.
 */
template<class T>
class SharedMember final {
    public:
        /** Using as a T gives you the underlying T object */
        virtual operator T& () const { return *ptr; }
        /** Using as an eris_id_t gives you the object's id */
        virtual operator eris_id_t () const { return ptr->id(); }
        /** Dereferencing gives you the underlying T */
        virtual T& operator * () const { return *ptr; }
        /** Dereferencing member access works on the underlying T */
        virtual T* operator -> () const { return ptr.get(); }

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

    private:
        SharedMember(T *m) : ptr(m) {}
        friend class Simulation;
};

}
