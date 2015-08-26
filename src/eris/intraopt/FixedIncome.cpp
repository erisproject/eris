#include <eris/intraopt/FixedIncome.hpp>
#include <eris/agent/AssetAgent.hpp>

namespace eris { namespace intraopt {

FixedIncome::FixedIncome(const agent::AssetAgent &agent, Bundle income) : income(income), agent_id_(agent) {}

void FixedIncome::intraInitialize() {
    simAgent<agent::AssetAgent>(agent_id_)->assets() += income;
}

} }
