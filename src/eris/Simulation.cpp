#include <eris/Member.hpp>
#include <eris/Simulation.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/InterOptimizer.hpp>
#include <algorithm>

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
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    a->simulation(shared_from_this(), id_next_++);
    agents_->insert(std::make_pair(a->id(), a));
    invalidateCache<Agent>();
}
// Assign an ID, set it, store the simulator, and insert into the good map
// This should be the *ONLY* place anything is ever added into goods_
void Simulation::insertGood(const SharedMember<Good> &g) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    g->simulation(shared_from_this(), id_next_++);
    goods_->insert(std::make_pair(g->id(), g));
    invalidateCache<Good>();
}
// Assign an ID, set it, store the simulator, and insert into the market map
// This should be the *ONLY* place anything is ever added into markets_
void Simulation::insertMarket(const SharedMember<Market> &m) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    m->simulation(shared_from_this(), id_next_++);
    markets_->insert(std::make_pair(m->id(), m));
    invalidateCache<Market>();
}
// Assign an ID, set it, store the optimizer, and insert into the intraopt map
// This should be the *ONLY* place anything is ever added into intraopts_
void Simulation::insertIntraOpt(const SharedMember<IntraOptimizer> &o) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    o->simulation(shared_from_this(), id_next_++);
    intraopts_->insert(std::make_pair(o->id(), o));
    invalidateCache<IntraOptimizer>();
}
// Assign an ID, set it, store the optimizer, and insert into the interopt map
// This should be the *ONLY* place anything is ever added into interopts_
void Simulation::insertInterOpt(const SharedMember<InterOptimizer> &o) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    o->simulation(shared_from_this(), id_next_++);
    interopts_->insert(std::make_pair(o->id(), o));
    invalidateCache<InterOptimizer>();
}

void Simulation::registerDependency(const eris_id_t &member, const eris_id_t &depends_on) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    depends_on_[depends_on].insert(member);
}

// This should be the *ONLY* place anything is ever removed from agents_
void Simulation::removeAgent(const eris_id_t &aid) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    auto mlock = agents_->at(aid)->readLock();
    agents_->at(aid)->simulation(nullptr, 0);
    agents_->erase(aid);
    removeDeps(aid);
    invalidateCache<Agent>();
}
void Simulation::removeGood(const eris_id_t &gid) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    auto mlock = goods_->at(gid)->readLock();
    goods_->at(gid)->simulation(nullptr, 0);
    goods_->erase(gid);
    removeDeps(gid);
    invalidateCache<Good>();
}
void Simulation::removeMarket(const eris_id_t &mid) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    auto mlock = markets_->at(mid)->readLock();
    markets_->at(mid)->simulation(nullptr, 0);
    markets_->erase(mid);
    removeDeps(mid);
    invalidateCache<Market>();
}
void Simulation::removeIntraOpt(const eris_id_t &oid) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    auto mlock = intraopts_->at(oid)->readLock();
    intraopts_->at(oid)->simulation(nullptr, 0);
    intraopts_->erase(oid);
    removeDeps(oid);
    invalidateCache<IntraOptimizer>();
}
void Simulation::removeInterOpt(const eris_id_t &oid) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    auto mlock = interopts_->at(oid)->readLock();
    interopts_->at(oid)->simulation(nullptr, 0);
    interopts_->erase(oid);
    removeDeps(oid);
    invalidateCache<InterOptimizer>();
}



void Simulation::removeDeps(const eris_id_t &member) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

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

void Simulation::maxThreads(unsigned long max_threads) {
    if (running_)
        throw std::runtime_error("Cannot change number of threads during a Simulation run() call");
    max_threads_ = max_threads;
}

void Simulation::nothr_stage(const RunStage &stage) {
    stage_ = stage;
    // Does the same as thr_stage() and thr_loop() for the current stage, but with all of the
    // threading code; this is substantially simpler code as a result.
    switch (stage_) {
        // Inter-period optimizer stages.  These are in order and are intentionally ifs, not else
        // ifs, because we they occur in order but new threads can enter anywhere along the process.
        case RunStage::inter_optimize:
            for (auto &pair : *interopts_) pair.second->optimize();
            break;

        case RunStage::inter_apply:
            for (auto &pair : *interopts_) pair.second->apply();
            break;

        case RunStage::inter_advance:
            for (auto &pair : *agents_) pair.second->advance();
            break;

        case RunStage::inter_postAdvance:
            for (auto &pair : *interopts_) pair.second->postAdvance();
            break;

            // Intra-period optimizations
        case RunStage::intra_initialize:
            for (auto &pair : *intraopts_) pair.second->initialize();
            break;

        case RunStage::intra_reset:
            for (auto &pair : *intraopts_) pair.second->reset();
            break;

        case RunStage::intra_optimize:
            for (auto &pair : *intraopts_) pair.second->optimize();
            break;

        case RunStage::intra_postOptimize:
            for (auto &pair : *intraopts_) {
                if (pair.second->postOptimize())
                    thr_redo_intra_ = true;
            }
            break;

        case RunStage::intra_apply:
            for (auto &pair : *intraopts_) pair.second->apply();
            break;

        case RunStage::kill:
        case RunStage::kill_all:
        case RunStage::idle:
            // None of these should happen when not using threads!
            throw std::runtime_error("Found thread-specific state (kill, kill_all, or idle) but threads are not in use!");
            break;
    }
}

void Simulation::thr_loop() {
    thr_sync_mutex_.lock();
    // Continually looks for the various stages and responds according.  Threads are allowed to
    // start work in any stage except idle.  The thread continues forever unless it sees a kill
    // stage with thr_kill_ set to its thread id.
    while (true) {
        switch (stage_) {
            // Every case should either exit the loop or wait
            case RunStage::kill:
                if (thr_kill_ == std::this_thread::get_id()) {
                    thr_sync_mutex_.unlock();
                    return; // Killed!
                }
                else {
                    // Otherwise it's someone else, so just wait until either another stage happens, or
                    // we get killed
                    thr_cv_stage_.wait(thr_sync_mutex_, [&] {
                            return stage_ != RunStage::kill or thr_kill_ == std::this_thread::get_id(); });
                }
                break;

            case RunStage::kill_all:
                thr_sync_mutex_.unlock();
                return; // Killed!
                break;

            // Inter-period optimizer stages.  These are in order and are intentionally ifs, not else
            // ifs, because we they occur in order but new threads can enter anywhere along the process.
            case RunStage::inter_optimize:
                thr_work_inter([](InterOptimizer &opt) { opt.optimize(); });
                thr_stage_finished();
                break;

            case RunStage::inter_apply:
                thr_work_inter([](InterOptimizer &opt) { opt.apply(); });
                thr_stage_finished();
                break;

            case RunStage::inter_advance:
                thr_work_agent([](Agent &agent) { agent.advance(); });
                thr_stage_finished();
                break;

            case RunStage::inter_postAdvance:
                thr_work_inter([](InterOptimizer &opt) { opt.postAdvance(); });
                thr_stage_finished();
                break;

            // Intra-period optimizations
            case RunStage::intra_initialize:
                thr_work_intra([](IntraOptimizer &opt) { opt.initialize(); });
                thr_stage_finished();
                break;

            case RunStage::intra_reset:
                thr_work_intra([](IntraOptimizer &opt) { opt.reset(); });
                thr_stage_finished();
                break;

            case RunStage::intra_optimize:
                thr_work_intra([](IntraOptimizer &opt) { opt.optimize(); });
                thr_stage_finished();
                break;

            case RunStage::intra_postOptimize:
                // Slightly trickier than the others: we need to signal a redo on the intra-optimizers
                // if any postOptimize returns false.
                thr_work_intra([this](IntraOptimizer &opt) {
                    if (opt.postOptimize()) { // Need a restart
                        thr_sync_mutex_.lock();
                        thr_redo_intra_ = true;
                        thr_sync_mutex_.unlock();
                    }
                });
                // After a postOptimize, we wait for *either* a return to reset or an apply
                thr_stage_finished();
                break;

            case RunStage::intra_apply:
                thr_work_intra([](IntraOptimizer &opt) { opt.apply(); });
                thr_stage_finished();
                break;

            case RunStage::idle:
                // Just wait for the state to change to something else.
                thr_wait();
                break;
        }

        if (maxThreads() == 0) return;
    }
}

void Simulation::thr_work_agent(std::function<void(Agent&)> work) {
    while (thr_q_agent_iter_ != thr_q_agent_end_) {
        auto &pair = *(thr_q_agent_iter_++);
        thr_sync_mutex_.unlock();
        work(pair.second);
        thr_sync_mutex_.lock();
    }
}
void Simulation::thr_work_inter(std::function<void(InterOptimizer&)> work) {
    while (thr_q_inter_iter_ != thr_q_inter_end_) {
        auto &pair = *(thr_q_inter_iter_++);
        thr_sync_mutex_.unlock();
        work(pair.second);
        thr_sync_mutex_.lock();
    }
}
void Simulation::thr_work_intra(std::function<void(IntraOptimizer&)> work) {
    while (thr_q_intra_iter_ != thr_q_intra_end_) {
        auto &pair = *(thr_q_intra_iter_++);
        thr_sync_mutex_.unlock();
        work(pair.second);
        thr_sync_mutex_.lock();
    }
}

void Simulation::thr_stage(MemberMap<Agent> &mem_map, const RunStage &stage) {
    {
        std::lock_guard<std::recursive_mutex> lock(member_mutex_);
        thr_q_size_ = mem_map.size();
        thr_q_agent_iter_ = mem_map.begin();
        thr_q_agent_end_  = mem_map.end();
    }

    thr_stage(stage);
}

void Simulation::thr_stage(MemberMap<InterOptimizer> &mem_map, const RunStage &stage) {
    {
        std::lock_guard<std::recursive_mutex> lock(member_mutex_);
        thr_q_size_ = mem_map.size();
        thr_q_inter_iter_ = mem_map.begin();
        thr_q_inter_end_  = mem_map.end();
    }

    thr_stage(stage);
}

void Simulation::thr_stage(MemberMap<IntraOptimizer> &mem_map, const RunStage &stage) {
    {
        std::lock_guard<std::recursive_mutex> lock(member_mutex_);
        thr_q_size_ = mem_map.size();
        thr_q_intra_iter_ = mem_map.begin();
        thr_q_intra_end_  = mem_map.end();
    }

    thr_stage(stage);
}

void Simulation::thr_stage(const RunStage &stage) {
    stage_ = stage;
    unsigned long max_threads = std::min(maxThreads(), thr_q_size_);

    while (thr_pool_.size() < max_threads) {
        thr_pool_.push_back(std::thread(&Simulation::thr_loop, this));
    }
    thr_done_ = 0;
    thr_cv_stage_.notify_all();

    thr_cv_done_.wait(thr_sync_mutex_, [this] { return thr_done_ >= thr_pool_.size(); });
}



void Simulation::run() {
    if (running_)
        throw std::runtime_error("Calling Simulation::run() recursively not permitted");
    running_ = true;

    if (maxThreads() > 0) thr_sync_mutex_.lock();
    stage_ = RunStage::idle;

    if (thr_pool_.size() > maxThreads()) {
        // Too many threads in the pool, kill some off
        for (unsigned int excess = thr_pool_.size() - maxThreads(); excess > 0; excess--) {
            auto &thr = thr_pool_.front();
            stage_ = RunStage::kill;
            thr_kill_ = thr.get_id();
            thr_sync_mutex_.unlock();
            thr_cv_stage_.notify_all();
            thr.join();
            thr_sync_mutex_.lock();
            thr_pool_.pop_front();
        }
    }

    if (++iteration_ > 1) {
        // Skip all this on the first iteration
        if (maxThreads() > 0) {
            thr_stage(*interopts_, RunStage::inter_optimize);
            thr_stage(*interopts_, RunStage::inter_apply);
            thr_stage(*agents_, RunStage::inter_advance);
            thr_stage(*interopts_, RunStage::inter_postAdvance);
        }
        else {
            nothr_stage(RunStage::inter_optimize);
            nothr_stage(RunStage::inter_apply);
            nothr_stage(RunStage::inter_advance);
            nothr_stage(RunStage::inter_postAdvance);
        }
    }

    intraopt_count = 0;

    if (maxThreads() > 0)
        thr_stage(*intraopts_, RunStage::intra_initialize);
    else
        nothr_stage(RunStage::intra_initialize);

    thr_redo_intra_ = true;
    while (thr_redo_intra_) {
        intraopt_count++;
        if (maxThreads() > 0) {
            thr_stage(*intraopts_, RunStage::intra_reset);
            thr_stage(*intraopts_, RunStage::intra_optimize);
            thr_redo_intra_ = false;
            thr_stage(*intraopts_, RunStage::intra_postOptimize);
        }
        else {
            nothr_stage(RunStage::intra_reset);
            nothr_stage(RunStage::intra_optimize);
            thr_redo_intra_ = false;
            nothr_stage(RunStage::intra_postOptimize);
        }
    }

    if (maxThreads() > 0)
        thr_stage(*intraopts_, RunStage::intra_apply);
    else
        nothr_stage(RunStage::intra_apply);

    stage_ = RunStage::idle;
    if (maxThreads() > 0) thr_sync_mutex_.unlock();
    running_ = false;
}

Simulation::~Simulation() {
    thr_sync_mutex_.lock();
    stage_ = RunStage::kill_all;
    thr_sync_mutex_.unlock();
    thr_cv_stage_.notify_all();

    for (auto &thr : thr_pool_) {
        thr.join();
    }
}

}
