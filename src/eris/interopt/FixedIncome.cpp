#include <eris/interopt/FixedIncome.hpp>

namespace eris { namespace interopt {

FixedIncome::FixedIncome(const agent::AssetAgent &agent, Bundle income) : income(income), agent_id_(agent) {}

void FixedIncome::interPostAdvance() {
    simAgent<agent::AssetAgent>(agent_id_)->assets() += income;
}

} }
