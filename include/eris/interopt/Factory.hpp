#pragma once
#include <eris/Member.hpp>
#include <eris/Optimize.hpp>

namespace eris { namespace interopt {

/** Interface for an inter-period "factory" class which creates and/or destroys members of the
 * simulation based on various criteria.  Typical use involves creating new Agents and removing
 * existing ones, though this Factory can be used for any type of Eris member.
 *
 * For example, a firm factory implementation could create new firms to serve a market if the firms
 * in that market had sufficiently high profits in the previous period, and could remove a random
 * firm from the market if profits were negative.
 *
 * As another example, a factory could create new goods and markets based probabilistically on a
 * level of research in the previous period(s).
 *
 * A factory is typically created and added at the same time via a call such as:
 *
 *     simulation->create<FactoryImplementation>()
 *
 * The implementing class typically has just three methods: needAction(), which determines when a
 * create/destroy action should be taken, and create() and destroy() which add or remove from the
 * simulation as needed.
 */
class Factory : public Member, public virtual OptApply {
    public:

        /** Called during the inter-period "optimize" phase to determine whether new objects should
         * be created or destroyed.
         *
         * If this returns a positive value \f$n\f$, create(n) will be called (during the interopt
         * "apply" phase) to create \f$n\f$ items.
         *
         * If a negative value \f$-n\f$ is returned, destroy(n) will be called (during the interopt
         * "apply" phase) to perform the destructions.
         *
         * If zero is returned, nothing happens in the apply phase.
         */
        virtual int needAction() = 0;

        /** Called during inter-period optimization.  Calls shouldCreate() to determine whether
         * create/destroy calls are needed during interApply().
         */
        virtual void interOptimize() override;

        /** Called during inter-period optimization.  If shouldCreated() returned a non-zero value,
         * this calls create(n) or destroy(n) the required number of times.
         */
        virtual void interApply() override;

        /** Called to perform creation.  Takes one argument, the number of items to be created.
         */
        virtual void create(unsigned int n) = 0;

        /** Called to perform destruction.  Takes one argument, the number of items to be destroyed.
         * Implementors without destructive capability should simple provide an empty implementation
         * of this method.
         */
        virtual void destroy(unsigned int n) = 0;

    private:
        int action_ = 0;
};

} }
