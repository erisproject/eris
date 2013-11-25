#pragma once
#include <eris/agent/AssetAgent.hpp>
#include <eris/Bundle.hpp>
#include <eris/Optimize.hpp>

namespace eris { namespace interopt {

/** Simple inter-period "optimization" class that simply adds a fixed bundle (i.e. income) to its
 * agent's assets at the beginning of each period.
 */
class FixedIncome : public Member, public virtual PostAdvance {
    public:
        /** Creates a new FixedIncome optimizer that adds the bundle income to the given agent at
         * the beginning of each period.
         */
        FixedIncome(const agent::AssetAgent &agent, Bundle income);

        /** Adds the income bundle to the agent's assets. */
        virtual void interPostAdvance() override;

        /** The bundle that is added at the beginning of each period. */
        Bundle income;

    private:
        const eris_id_t agent_id_;
};

} }
