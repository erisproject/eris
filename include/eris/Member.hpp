#pragma once
#include <eris/types.hpp>
#include <memory>

namespace eris {

template <class T> class SharedMember;
class Simulation;
class Agent;
class Good;
class Market;
class IntraOptimizer;
class InterOptimizer;

/** Base class for "members" of a simulation: goods, agents, markets, and optimizers.  This class
 * provides an id, a weak reference to the owning simulation object, and a < operator comparing ids
 * (so that ordering works).
 */
class Member {
    public:
        virtual ~Member() = default;
        /** Returns the eris_id_t ID of this member.  Returns 0 if this Member instance has not yet
         * been added to a Simulation.
         */
        eris_id_t id() const { return id_; }
        /** A Member object can be used anywhere an eris_id_t value is called for and will evaluate
         * to the Member's ID.
         */
        operator eris_id_t() const { return id_; }

        /** Creates and returns a shared pointer to the simulation this object belongs.  Returns a
         * null shared_ptr if there is no (current) simulation.
         */
        virtual std::shared_ptr<Simulation> simulation() const;

        /** Shortcut for `member.simulation()->agent<A>()` */
        template <class A = Agent> SharedMember<A> simAgent(eris_id_t aid) const;
        /** Shortcut for `member.simulation()->good<G>()` */
        template <class G = Good> SharedMember<G> simGood(eris_id_t gid) const;
        /** Shortcut for `member.simulation()->market<M>()` */
        template <class M = Market> SharedMember<M> simMarket(eris_id_t mid) const;
        /** Shortcut for `member.simulation()->intraOpt<I>()` */
        template <class I = IntraOptimizer> SharedMember<I> simIntraOpt(eris_id_t oid) const;
        /** Shortcut for `member.simulation()->interOpt<I>()` */
        template <class I = InterOptimizer> SharedMember<I> simInterOpt(eris_id_t oid) const;


    protected:
        /** Called (by Simulation) to store a weak pointer to the simulation this member belongs to
         * and the member's id.  This is called once when the Member is added to a simulation, and
         * again (with a null shared pointer and id of 0) when the Member is removed from a
         * Simulation.
         */
        void simulation(const std::shared_ptr<Simulation> &sim, const eris_id_t &id);
        friend eris::Simulation;

        /** Virtual method called just after the member is added to a Simulation object.  The
         * default implementation does nothing.  This method is typically used to record a
         * dependency in the simulation, but can also do initial setup.
         */
        virtual void added();

        /** Virtual method called just after the member is removed from a Simulation object.  The
         * default implementation does nothing.  The simulation() and id() methods are still
         * available, but the simulation no longer references this Member.
         *
         * Note that any registered dependencies may not exist (in particular when the removal is
         * occuring as the result of a cascading dependency removal).  In other words, if this
         * Member has registered a dependency on B, when B is removed, this Member will also be
         * removed *after* B has been removed from the Simulation.  Thus classes overriding this
         * method must *not* make use of dependent objects.
         */
        virtual void removed();

        /** Records a dependency with the Simulation object.  This should not be called until after
         * simulation() has been called, and is typically invoked in an overridden added() method.
         *
         * This is simply an alias for Simulation::registerDependency; the two following statements
         * are exactly equivalent:
         *
         *     shared_member->dependsOn(other_member);
         *     shared_member->simulation()->registerDependency(shared_member, other_member);
         */
        void dependsOn(const eris_id_t &id);

        /** Helper method to ensure that the passed-in SharedMember is a subclass of the templated
         * class.  If not, this throws an invalid_argument exception with the given message.
         */
        template<class B, class C>
        void requireInstanceOf(const SharedMember<C> &obj, const std::string &error) {
            if (!dynamic_cast<B*>(obj.ptr.get())) throw std::invalid_argument(error);
        }

    private:
        eris_id_t id_ = 0;
        /** Stores a weak pointer to the simulation this Member belongs to. */
        std::weak_ptr<eris::Simulation> simulation_;
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
 * another compatible SharedMember instance), but are created via the various add* and clone*
 * methods of Simulation.
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

// This has to be down here because there is a circular dependency between Simulation and Member:
// each has templated methods requiring the definition of the other.
#include <eris/Simulation.hpp>

namespace eris {
template <class A> SharedMember<A> Member::simAgent(eris_id_t aid) const {
    return simulation()->agent<A>(aid);
}
template <class G> SharedMember<G> Member::simGood(eris_id_t gid) const {
    return simulation()->good<G>(gid);
}
template <class M> SharedMember<M> Member::simMarket(eris_id_t mid) const {
    return simulation()->market<M>(mid);
}
template <class I> SharedMember<I> Member::simIntraOpt(eris_id_t oid) const {
    return simulation()->intraOpt<I>(oid);
}
template <class I> SharedMember<I> Member::simInterOpt(eris_id_t oid) const {
    return simulation()->interOpt<I>(oid);
}
}
