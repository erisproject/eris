#pragma once
#include <eris/types.hpp>
#include <eris/SharedMember.hpp>
#include <stdexcept>

namespace eris {

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
