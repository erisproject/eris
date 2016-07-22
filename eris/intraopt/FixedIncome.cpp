#include <eris/intraopt/FixedIncome.hpp>
#include <eris/Agent.hpp>

namespace eris { namespace intraopt {

FixedIncome::FixedIncome(const Agent &agent, Bundle income) : income(income), agent_id_(agent) {}

void FixedIncome::intraInitialize() {
    simAgent<Agent>(agent_id_)->assets += income;
}

} }
