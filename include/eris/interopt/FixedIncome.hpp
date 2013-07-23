#pragma once
#include <eris/Agent.hpp>
#include <eris/Bundle.hpp>
#include <eris/InterOptimizer.hpp>

namespace eris { namespace interopt {

/** Simple inter-period "optimization" class that simply adds a fixed bundle (i.e. income) to its
 * agent's assets at the beginning of each period.
 */
class FixedIncome : public InterOptimizer {
    public:
        /** Creates a new FixedIncome optimizer that adds the bundle income to the given agent at
         * the beginning of each period.
         */
        FixedIncome(const Agent &agent, Bundle income);

        /** Does nothing as no optimization is needed. */
        virtual void optimize() const override;

        /** Adds the income bundle to the agent's assets. */
        virtual void postAdvance() override;

        /** The bundle that is added at the beginning of each period. */
        Bundle income;

    private:
        const eris_id_t agent_id_;
};

} }
