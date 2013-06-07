#include <eris/Optimizer.hpp>

namespace eris {

// The sim argument below will throw a std::bad_weak_ptr exception if the simulation pointer is expired
Optimizer::Optimizer(const Agent &a) : agent_id(a), sim(std::shared_ptr<Simulation>(a.simulation())) {}

std::shared_ptr<Simulation> Optimizer::simulation() {
    return std::shared_ptr<Simulation>(sim);
}

SharedMember<Agent> Optimizer::agent() {
    return simulation()->agent(agent_id);
}

BundleNegative& Optimizer::assets() {
    return agent()->assets();
}

void Optimizer::reset() {}

}
