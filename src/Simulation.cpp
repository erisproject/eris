#include <eris/Simulation.hpp>

namespace eris {

// Assign an ID, set it, store the simulator, and insert into the agent map
void Simulation::insertAgent(const SharedMember<Agent> &a) {
    a->simulation(shared_from_this(), id_next_++);
    agents_.insert(std::make_pair(a->id(), a));
}
// Assign an ID, set it, store the simulator, and insert into the good map
void Simulation::insertGood(const SharedMember<Good> &g) {
    g->simulation(shared_from_this(), id_next_++);
    goods_.insert(std::make_pair(g->id(), g));
}
// Assign an ID, set it, store the simulator, and insert into the market map
void Simulation::insertMarket(const SharedMember<Market> &m) {
    m->simulation(shared_from_this(), id_next_++);
    markets_.insert(std::make_pair(m->id(), m));
}
// Assign an ID, set it, store the optimizer, and insert into the intraopt map
void Simulation::insertIntraOpt(const SharedMember<IntraOptimizer> &o) {
    o->simulation(shared_from_this(), id_next_++);
    intraopts_.insert(std::make_pair(o->id(), o));
}
// Assign an ID, set it, store the optimizer, and insert into the interopt map
void Simulation::insertInterOpt(const SharedMember<InterOptimizer> &o) {
    o->simulation(shared_from_this(), id_next_++);
    interopts_.insert(std::make_pair(o->id(), o));
}

void Simulation::registerDependency(const eris_id_t &member, const eris_id_t &depends_on) {
    depends_on_[depends_on].insert(member);
}

void Simulation::removeAgent(const eris_id_t &aid) {
    agents_.at(aid)->simulation(nullptr, 0);
    agents_.erase(aid);
    removeDeps(aid);
}
void Simulation::removeGood(const eris_id_t &gid) {
    goods_.at(gid)->simulation(nullptr, 0);
    goods_.erase(gid);
    removeDeps(gid);
}
void Simulation::removeMarket(const eris_id_t &mid) {
    markets_.at(mid)->simulation(nullptr, 0);
    markets_.erase(mid);
    removeDeps(mid);
}
void Simulation::removeIntraOpt(const eris_id_t &oid) {
    intraopts_.at(oid)->simulation(nullptr, 0);
    intraopts_.erase(oid);
    removeDeps(oid);
}
void Simulation::removeInterOpt(const eris_id_t &oid) {
    interopts_.at(oid)->simulation(nullptr, 0);
    interopts_.erase(oid);
    removeDeps(oid);
}

void Simulation::removeDeps(const eris_id_t &member) {
    if (!depends_on_.count(member)) return;

    // Remove the dependents before iterating, to break potential dependency loops
    const auto deps = depends_on_.at(member);
    depends_on_.erase(member);

    for (const auto &dep : deps) {
        if      (   agents_.count(dep))    removeAgent(dep);
        else if (    goods_.count(dep))     removeGood(dep);
        else if (  markets_.count(dep))   removeMarket(dep);
        else if (intraopts_.count(dep)) removeIntraOpt(dep);
        else if (interopts_.count(dep)) removeInterOpt(dep);
        // Otherwise it's already removed (possibly because of some nested dependencies), so don't
        // worry about it.
    }
}

const Simulation::AgentMap& Simulation::agents()       { return agents_;    }
const Simulation::GoodMap& Simulation::goods()         { return goods_;     }
const Simulation::MarketMap& Simulation::markets()     { return markets_;   }
const Simulation::IntraOptMap& Simulation::intraOpts() { return intraopts_; }
const Simulation::InterOptMap& Simulation::interOpts() { return interopts_; }

const Simulation::DepMap& Simulation::deps() { return depends_on_; }

void Simulation::run() {
    if (++iteration_ > 1) {
        // Skip all this on the first iteration

        for (auto &inter : interOpts()) {
            inter.second->optimize();
        }

        for (auto &inter : interOpts()) {
            inter.second->apply();
        }

        for (auto &agent : agents()) {
            agent.second->advance();
        }

        for (auto &inter : interOpts()) {
            inter.second->postAdvance();
        }
    }

    intraopt_count = 0;

    for (auto &intra : intraOpts()) {
        intra.second->initialize();
    }

    bool done = false;
    while (!done) {
        done = true;
        ++intraopt_count;

        for (auto &intra : intraOpts()) {
            intra.second->reset();
        }

        for (auto &intra : intraOpts()) {
            intra.second->optimize();
        }

        for (auto &intra : intraOpts()) {
            if (intra.second->postOptimize())
                done = false;
            // Deliberately do *not* short-circuit here, because we want intraopt optimizers to work
            // in parallel, e.g. if there are 10 markets, each taking ~10 postOptimize steps, we
            // want them happening in parallel: otherwise we could get 10^10 operations (since each
            // later optimization might require another 10 steps of the already-optimized ones).
            //
            // Realistically, optimization also should not assume to be operating in a vacuum, and
            // intraopt optimizers should be aware of that.
        }
    }

    // We got through optimize() and postOptimize() with false returns (i.e. nothing changed), so
    // now let them apply whatever they calculated
    for (auto intra : intraOpts())
        intra.second->apply();
}

}
