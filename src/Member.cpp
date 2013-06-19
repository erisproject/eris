#include <eris/Member.hpp>
#include <eris/Simulation.hpp>

namespace eris {

void Member::dependsOn(const eris_id_t &id) {
    simulation()->registerDependency(*this, id);
}


void Member::simulation(const std::shared_ptr<Simulation> &sim, const eris_id_t &id) {
    if (id == 0) removed();
    simulation_ = sim;
    id_ = id;
    if (id > 0) added();
}


std::shared_ptr<Simulation> Member::simulation() const {
    return simulation_.lock();
}

void Member::added() {}
void Member::removed() {}

}
