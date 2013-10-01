#include <eris/Member.hpp>
#include <eris/Simulation.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/InterOptimizer.hpp>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include <iostream>

namespace eris {

Simulation::Simulation() :
    agents_(new MemberMap<Agent>()),
    goods_(new MemberMap<Good>()),
    markets_(new MemberMap<Market>()),
    intraopts_(new MemberMap<IntraOptimizer>()),
    interopts_(new MemberMap<InterOptimizer>())
    {}

// Assign an ID, set it, store the simulator, and insert into the agent map
// This should be the *ONLY* place anything is ever added into agents_
void Simulation::insertAgent(const SharedMember<Agent> &a) {
    a->simulation(shared_from_this(), id_next_++);
    agents_->insert(std::make_pair(a->id(), a));
    invalidateCache<Agent>();
}
// Assign an ID, set it, store the simulator, and insert into the good map
// This should be the *ONLY* place anything is ever added into goods_
void Simulation::insertGood(const SharedMember<Good> &g) {
    g->simulation(shared_from_this(), id_next_++);
    goods_->insert(std::make_pair(g->id(), g));
    invalidateCache<Good>();
}
// Assign an ID, set it, store the simulator, and insert into the market map
// This should be the *ONLY* place anything is ever added into markets_
void Simulation::insertMarket(const SharedMember<Market> &m) {
    m->simulation(shared_from_this(), id_next_++);
    markets_->insert(std::make_pair(m->id(), m));
    invalidateCache<Market>();
}
// Assign an ID, set it, store the optimizer, and insert into the intraopt map
// This should be the *ONLY* place anything is ever added into intraopts_
void Simulation::insertIntraOpt(const SharedMember<IntraOptimizer> &o) {
    o->simulation(shared_from_this(), id_next_++);
    intraopts_->insert(std::make_pair(o->id(), o));
    invalidateCache<IntraOptimizer>();
}
// Assign an ID, set it, store the optimizer, and insert into the interopt map
// This should be the *ONLY* place anything is ever added into interopts_
void Simulation::insertInterOpt(const SharedMember<InterOptimizer> &o) {
    o->simulation(shared_from_this(), id_next_++);
    interopts_->insert(std::make_pair(o->id(), o));
    invalidateCache<InterOptimizer>();
}

void Simulation::registerDependency(const eris_id_t &member, const eris_id_t &depends_on) {
    depends_on_[depends_on].insert(member);
}

// This should be the *ONLY* place anything is ever removed from agents_
void Simulation::removeAgent(const eris_id_t &aid) {
    agents_->at(aid)->simulation(nullptr, 0);
    agents_->erase(aid);
    removeDeps(aid);
    invalidateCache<Agent>();
}
void Simulation::removeGood(const eris_id_t &gid) {
    goods_->at(gid)->simulation(nullptr, 0);
    goods_->erase(gid);
    removeDeps(gid);
    invalidateCache<Good>();
}
void Simulation::removeMarket(const eris_id_t &mid) {
    markets_->at(mid)->simulation(nullptr, 0);
    markets_->erase(mid);
    removeDeps(mid);
    invalidateCache<Market>();
}
void Simulation::removeIntraOpt(const eris_id_t &oid) {
    intraopts_->at(oid)->simulation(nullptr, 0);
    intraopts_->erase(oid);
    removeDeps(oid);
    invalidateCache<IntraOptimizer>();
}
void Simulation::removeInterOpt(const eris_id_t &oid) {
    interopts_->at(oid)->simulation(nullptr, 0);
    interopts_->erase(oid);
    removeDeps(oid);
    invalidateCache<InterOptimizer>();
}

void Simulation::removeDeps(const eris_id_t &member) {
    if (!depends_on_.count(member)) return;

    // Remove the dependents before iterating, to break potential dependency loops
    const auto deps = depends_on_.at(member);
    depends_on_.erase(member);

    for (const auto &dep : deps) {
        if      (   agents_->count(dep))    removeAgent(dep);
        else if (    goods_->count(dep))     removeGood(dep);
        else if (  markets_->count(dep))   removeMarket(dep);
        else if (intraopts_->count(dep)) removeIntraOpt(dep);
        else if (interopts_->count(dep)) removeInterOpt(dep);
        // Otherwise it's already removed (possibly because of some nested dependencies), so don't
        // worry about it.
    }
}

const Simulation::MemberMap<Agent>& Simulation::agents()   { return *agents_;  }
const Simulation::MemberMap<Good>& Simulation::goods()     { return *goods_;   }
const Simulation::MemberMap<Market>& Simulation::markets() { return *markets_; }
const Simulation::MemberMap<IntraOptimizer>& Simulation::intraOpts() { return *intraopts_; }
const Simulation::MemberMap<InterOptimizer>& Simulation::interOpts() { return *interopts_; }

const Simulation::DepMap& Simulation::deps() { return depends_on_; }

void Simulation::run() {

    std::list<std::thread> threads;

    // Set up a mutex for thread/master synchronization; this also guards stage/done_stage, below.
    std::mutex sync_mutex;

    // CVs for signalling stage finished and next stage messages
    std::condition_variable stage_cv, cont_opt_cv, cont_ag_cv;

    // The current stage, controlled by the master thread, read by the slaves
    RunStage stage = RunStage::inter_optimize;

    // The number finished the current stage, incremented by slaves, read and reset to 0 by the master
    unsigned int done_stage = 0;
    bool need_redo = true;
    unsigned int opt_threads = 0, agent_threads = 0;

    // FIXME: this is probably not a great threading model: creating one thread
    // per interopt object is going to be messy and wasteful, especially when
    // there are many more interopt objects than CPUs.
    //
    // Improvements to be made here:
    // - a thread pool of a configurable size
    // - creating threads (or a thread pool) once, instead of once per iteration


    auto finished_until = [&done_stage, &stage, &stage_cv, &cont_opt_cv]
        (const RunStage &need_stage, std::unique_lock<std::mutex> &lock) -> void {
            if (not lock) lock.lock();
            done_stage++;
            stage_cv.notify_one();
            cont_opt_cv.wait(lock, [&] { return stage == need_stage; });
            lock.unlock();
        };

    if (++iteration_ > 1) {
        // Skip all this on the first iteration

        for (auto &inter : interOpts()) {
            auto opt = inter.second;
            threads.push_back(std::thread([opt, &stage, &sync_mutex, &stage_cv, &cont_opt_cv, &done_stage, &finished_until]() {
                try {
                    opt->optimize();

                    std::unique_lock<std::mutex> lock(sync_mutex);
                    finished_until(RunStage::inter_apply, lock);

                    opt->apply();

                    finished_until(RunStage::inter_postAdvance, lock);

                    opt->postAdvance();
                } catch (std::exception &e) {
                    std::cerr << "Thread exception: " << e.what() << "\n" << std::flush;
                }
            }));
            opt_threads++;
        }

        for (auto &ag : agents()) {
            auto agent = ag.second;
            threads.push_back(std::thread([agent, &stage, &sync_mutex, &stage_cv, &cont_ag_cv, &done_stage]() {
                try {
                    std::unique_lock<std::mutex> lock(sync_mutex);
                    if (stage != RunStage::inter_advance)
                        cont_ag_cv.wait(lock, [&stage] { return stage == RunStage::inter_advance; });
                    lock.unlock();

                    agent->advance();

                    lock.lock();
                    done_stage++;
                    lock.unlock();
                    stage_cv.notify_one();
                } catch (std::exception &e) {
                    std::cerr << "Thread exception: " << e.what() << "\n" << std::flush;
                }
            }));
            agent_threads++;
        }

        // Wait for optimizations to finish
        std::unique_lock<std::mutex> lock(sync_mutex);
        if (done_stage < opt_threads)
            stage_cv.wait(lock, [&] { return done_stage == opt_threads; });
        
        // Start apply() stage
        done_stage = 0;
        stage = RunStage::inter_apply;
        cont_opt_cv.notify_all();
        stage_cv.wait(lock, [&] { return done_stage == opt_threads; });

        // Start advance() stage
        done_stage = 0;
        stage = RunStage::inter_advance;
        cont_ag_cv.notify_all();
        stage_cv.wait(lock, [&] { return done_stage == agent_threads; });

        // Start postAdvance() stage
        done_stage = 0;
        stage = RunStage::inter_postAdvance;
        cont_opt_cv.notify_all();
        lock.unlock();

        // Rejoin all threads
        for (auto &t : threads)
            if (t.joinable()) t.join();

        threads.clear();

    }

    intraopt_count = 0;
    stage = RunStage::intra_initialize;
    done_stage = 0;
    opt_threads = 0;

    for (auto &intra : intraOpts()) {
        std::cout << "ASDF " << opt_threads << "\n";
        auto opt = intra.second;
        threads.push_back(std::thread([opt, &stage, &sync_mutex, &stage_cv, &cont_opt_cv, &done_stage, &need_redo, &finished_until]() {

                std::cout << "initialize\n" << std::flush;
                opt->initialize();

                std::unique_lock<std::mutex> lock(sync_mutex);
                std::cout << "Waiting for reset\n" << std::flush;
                finished_until(RunStage::intra_reset, lock);
                std::cout << "reset stage starts now!\n";
                std::cout << "need_redo=" << need_redo << "\n" << std::flush;
                
                bool done = false;
                while (not done) {
                    std::cout << "reset\n" << std::flush;
                    opt->reset();

                    std::cout << "Waiting for optimize\n" << std::flush;
                    finished_until(RunStage::intra_optimize, lock);

                    std::cout << "optimize()\n" << std::flush;
                    opt->optimize();

                    std::cout << "Waiting for postOptimize\n" << std::flush;
                    finished_until(RunStage::intra_postOptimize, lock);

                    std::cout << "postOptimize()\n" << std::flush;
                    bool retry = opt->postOptimize();
                    lock.lock();
                    if (retry) need_redo = true;
                    done_stage++;
                    stage_cv.notify_one();
                    std::cout << "waiting for reset or apply\n" << std::flush;
                    cont_opt_cv.wait(lock, [&] { return stage == RunStage::intra_apply or stage == RunStage::intra_reset; });
                    lock.unlock();
                    std::cout << "Done waiting: " << (stage == RunStage::intra_apply ? "apply" : "reset") << "\n" << std::flush;
                    std::cout << "need_redo: " << need_redo << "\n" << std::flush;
                    if (stage == RunStage::intra_apply)
                        done = true;
                }

                opt->apply();
        }));
        opt_threads++;
    }

    std::unique_lock<std::mutex> lock(sync_mutex);
    // Wait for initialize
    if (done_stage < opt_threads)
        stage_cv.wait(lock, [&] { return done_stage == opt_threads; });

    need_redo = true;
    while (need_redo) {
        for (auto stg : { RunStage::intra_reset, RunStage::intra_optimize, RunStage::intra_postOptimize }) {
            if (stage == RunStage::intra_postOptimize) need_redo = false;
            std::cout << "main thread starting stage " << (stg ==
                    RunStage::intra_reset ? "reset" : stg ==
                    RunStage::intra_optimize ? "optimize" : stg ==
                    RunStage::intra_postOptimize ? "postOptimize" :
                    "(unknown)") << "; need_redo=" << need_redo << "\n" << std::flush;
            done_stage = 0;
            stage = stg;
            cont_opt_cv.notify_all();
            std::cout << "Notified all, waiting for next stage to finish\n" << std::flush;
            stage_cv.wait(lock, [&] {
                    std::cout << "done_stage=" << done_stage << ", opt_threads=" << opt_threads << "\n" << std::flush;
                    return done_stage == opt_threads; });
            std::cout << "stage over.  need_redo=" << need_redo << "\n" << std::flush;
        }
    }

    done_stage = 0;
    stage = RunStage::intra_apply;
    cont_opt_cv.notify_all();
    lock.unlock();

    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }

    threads.clear();
}


}
