#include <eris/interopt/FixedIncome.hpp>

namespace eris { namespace interopt {

FixedIncome::FixedIncome(const Agent &agent, Bundle income) : income(income), agent_id_(agent) {}

void FixedIncome::optimize() const {}

void FixedIncome::postAdvance() {
    simAgent(agent_id_)->assets() += income;
}

} }
