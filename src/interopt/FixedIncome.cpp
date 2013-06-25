#include <eris/interopt/FixedIncome.hpp>
#include <iostream>

namespace eris { namespace interopt {

FixedIncome::FixedIncome(const Agent &agent, Bundle income) : income(income), agent_id_(agent) {}

void FixedIncome::optimize() const {}

void FixedIncome::apply() {
    simulation()->agent(agent_id_)->assets() += income;
}

} }
