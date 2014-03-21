#include <eris/Member.hpp>
#include <eris/Simulation.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/Optimize.hpp>
#include <algorithm>

namespace eris {

void Simulation::registerDependency(const eris_id_t &member, const eris_id_t &depends_on) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    depends_on_[depends_on].insert(member);
}

void Simulation::registerWeakDependency(const eris_id_t &member, const eris_id_t &depends_on) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    weak_dep_[depends_on].insert(member);
}

void Simulation::insert(const SharedMember<Member> &member) {
    if (dynamic_cast<Agent*>(member.ptr_.get())) insertAgent(member);
    else if (dynamic_cast<Good*>(member.ptr_.get())) insertGood(member);
    else if (dynamic_cast<Market*>(member.ptr_.get())) insertMarket(member);
    else insertOther(member);
}

// Macro for the 4 nearly-identical versions of these two functions.  When adding to the simulation,
// we need to assign an eris_id_t, give a reference to the simulation to the object, insert into
// agents_/goods_/markets_/others_, register any optimization implementations, and invalidate the
// associated filter cache.  When removing, we need to undo all of the above.
// This should be the *ONLY* place anything is ever added or removed from agents_, goods_, markets_,
// and others_
//
// Searching help:
// insertAgent() insertGood() insertMarket() insertOther()
// removeAgent() removeGood() removeMarket() removeOther()
#define ERIS_SIM_INSERT_REMOVE_MEMBER(TYPE, CLASS, MAP)\
void Simulation::insert##TYPE(const SharedMember<CLASS> &member) {\
    std::lock_guard<std::recursive_mutex> mbr_lock(member_mutex_);\
    member->simulation(shared_from_this(), id_next_++);\
    MAP.insert(std::make_pair(member->id(), member));\
    insertOptimizers(member);\
    invalidateCache<CLASS>();\
}\
void Simulation::remove##TYPE(const eris_id_t &id) {\
    std::lock_guard<std::recursive_mutex> mbr_lock(member_mutex_);\
    auto &member = MAP.at(id);\
    auto lock = member->writeLock();\
    removeOptimizers(member);\
    member->simulation(nullptr, 0);\
    MAP.erase(id);\
    removeDeps(id);\
    notifyWeakDeps(member, id);\
    invalidateCache<CLASS>();\
}
ERIS_SIM_INSERT_REMOVE_MEMBER(Agent,  Agent,  agents_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Good,   Good,   goods_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Market, Market, markets_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Other,  Member, others_)
#undef ERIS_SIM_INSERT_REMOVE_MEMBER

// More searching help: these are in eris/Simulation.hpp:
// agent() agents() good() goods() market() markets() other() others()

void Simulation::remove(const eris_id_t &id) {
    if (agents_.count(id)) removeAgent(id);
    else if (goods_.count(id)) removeGood(id);
    else if (markets_.count(id)) removeMarket(id);
    else if (others_.count(id)) removeOther(id);
    else throw std::out_of_range("eris_id_t to be removed does not exist");
}

void Simulation::insertOptimizers(const SharedMember<Member> &member) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    Member *mem = member.ptr_.get();
#define ERIS_SIM_INSERT_OPTIMIZER(TYPE, STAGE)\
    if (dynamic_cast<TYPE##opt::STAGE*>(mem)) {\
        optimizers_[(int) RunStage::TYPE##_##STAGE].emplace(member);\
        shared_q_cache_[(int) RunStage::TYPE##_##STAGE].reset();\
    }
    ERIS_SIM_INSERT_OPTIMIZER(inter, Optimize)
    ERIS_SIM_INSERT_OPTIMIZER(inter, Apply)
    ERIS_SIM_INSERT_OPTIMIZER(inter, Advance)

    ERIS_SIM_INSERT_OPTIMIZER(intra, Initialize)
    ERIS_SIM_INSERT_OPTIMIZER(intra, Reset)
    ERIS_SIM_INSERT_OPTIMIZER(intra, Optimize)
    ERIS_SIM_INSERT_OPTIMIZER(intra, Reoptimize)
    ERIS_SIM_INSERT_OPTIMIZER(intra, Apply)
    ERIS_SIM_INSERT_OPTIMIZER(intra, Finish)
#undef ERIS_SIM_INSERT_OPTIMIZER
}
void Simulation::removeOptimizers(const SharedMember<Member> &member) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    for (int i = optimizers_.size()-1; i >= 0; i--) {
        if (optimizers_[i].erase(member) > 0) {
            shared_q_cache_[i].reset();
        }
    }
}

void Simulation::removeDeps(const eris_id_t &member) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    if (!depends_on_.count(member)) return;

    // Remove the dependents before iterating, to break potential dependency loops
    const auto deps = depends_on_[member];
    depends_on_.erase(member);

    for (const auto &dep : deps) {
        if      ( agents_.count(dep))  removeAgent(dep);
        else if (  goods_.count(dep))   removeGood(dep);
        else if (markets_.count(dep)) removeMarket(dep);
        else if ( others_.count(dep))  removeOther(dep);
        // Otherwise it's already removed (possibly because of some nested dependencies), so don't
        // worry about it.
    }
}

void Simulation::notifyWeakDeps(SharedMember<Member> member, const eris_id_t &old_id) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);

    if (!weak_dep_.count(old_id)) return;

    // member is already removed, so now clean it out of the weak_dep_
    const auto weak_deps = weak_dep_[old_id];
    weak_dep_.erase(old_id);

    for (const auto &dep : weak_deps) {
        SharedMember<Member> dep_mem;
        if      ( agents_.count(dep)) dep_mem = agents_[dep];
        else if (  goods_.count(dep)) dep_mem = goods_[dep];
        else if (markets_.count(dep)) dep_mem = markets_[dep];
        else if ( others_.count(dep)) dep_mem = others_[dep];
        else continue; // The weak dep is no longer around

        dep_mem->weakDepRemoved(member, old_id);
    }
}

void Simulation::maxThreads(unsigned long max_threads) {
    if (running_)
        throw std::runtime_error("Cannot change number of threads during a Simulation run() call");
    max_threads_ = max_threads;
}

void Simulation::thr_loop() {
    // Continually looks for the various stages and responds according.  Threads are allowed to
    // start work in any stage except idle.  The thread continues forever unless it sees a kill
    // stage with thr_kill_ set to its thread id.
    while (true) {
        RunStage curr_stage = stage_;
        switch (curr_stage) {
            // Every case should either exit the loop or wait
            case RunStage::idle:
                // Just wait for the state to change to something else.
                thr_wait(curr_stage);
                break;
            case RunStage::kill:
                if (thr_kill_ == std::this_thread::get_id()) {
                    return; // Killed!
                }
                else {
                    // Otherwise it's someone else, so just wait until either another stage happens, or
                    // we get killed
                    std::unique_lock<std::mutex> lock(stage_mutex_);
                    thr_cv_stage_.wait(lock, [&] {
                            return stage_ != RunStage::kill or thr_kill_ == std::this_thread::get_id(); });
                }
                break;

            case RunStage::kill_all:
                return; // Killed!
                break;

#define ERIS_SIM_STAGE_CASE(TYPE, STAGE)\
            case RunStage::TYPE##_##STAGE:\
                thr_work<TYPE##opt::STAGE>([](TYPE##opt::STAGE &o) { o.TYPE##STAGE(); });\
                thr_stage_finished(curr_stage);\
                break

            // Inter-period optimizer stages
            ERIS_SIM_STAGE_CASE(inter, Optimize);
            ERIS_SIM_STAGE_CASE(inter, Apply);
            ERIS_SIM_STAGE_CASE(inter, Advance);

            // Intra-period optimizer stages
            ERIS_SIM_STAGE_CASE(intra, Initialize);
            ERIS_SIM_STAGE_CASE(intra, Reset);
            ERIS_SIM_STAGE_CASE(intra, Optimize);
            ERIS_SIM_STAGE_CASE(intra, Apply);
            ERIS_SIM_STAGE_CASE(intra, Finish);
#undef ERIS_SIM_STAGE_CASE
            case RunStage::intra_Reoptimize:
                // Slightly trickier than the others: we need to signal a redo on the intra-optimizers
                // if any reoptimize returns false.
                thr_work<intraopt::Reoptimize>([this](intraopt::Reoptimize &opt) {
                    if (opt.intraReoptimize()) // Need a restart
                        thr_redo_intra_ = true;
                });
                thr_stage_finished(curr_stage);
                break;
        }
    }
}

template <class Opt>
void Simulation::thr_work(const std::function<void(Opt&)> &work) {
    auto qsize = shared_q_->size();
    size_t i;
    while ((i = shared_q_next_++) < qsize) {
        work(*(dynamic_cast<Opt*>(shared_q_->operator[](i).ptr_.get())));
    }
}

void Simulation::thr_stage(const RunStage &stage) {
    switch (stage) {
        case RunStage::inter_Optimize:
        case RunStage::inter_Apply:
        case RunStage::inter_Advance:
        case RunStage::intra_Initialize:
        case RunStage::intra_Reset:
        case RunStage::intra_Optimize:
        case RunStage::intra_Reoptimize:
        case RunStage::intra_Apply:
        case RunStage::intra_Finish:
            {
                std::lock_guard<std::recursive_mutex> lock(member_mutex_);

                auto &shared_cache = shared_q_cache_[(int) stage];
                if (not shared_cache) {
                    shared_cache.reset(new std::vector<SharedMember<Member>>());
                    const auto &opts = optimizers_[(int) stage];
                    shared_cache->reserve(opts.size());
                    shared_cache->insert(shared_cache->end(), opts.cbegin(), opts.cend());
                }
                shared_q_ = shared_cache;
                shared_q_next_ = 0;
            }
            break;
        default:
            throw std::runtime_error("thr_stage called with non-stage RunStage");
    }

    if (maxThreads() == 0) {
        // Not using threads; call thr_work directly
        switch (stage) {
#define ERIS_SIM_NOTHR_WORK(TYPE, STAGE)\
            case RunStage::TYPE##_##STAGE:\
                thr_work<TYPE##opt::STAGE>([](TYPE##opt::STAGE &o) { o.TYPE##STAGE(); });\
                break;
            ERIS_SIM_NOTHR_WORK(inter, Optimize)
            ERIS_SIM_NOTHR_WORK(inter, Apply)
            ERIS_SIM_NOTHR_WORK(inter, Advance)

            ERIS_SIM_NOTHR_WORK(intra, Initialize)
            ERIS_SIM_NOTHR_WORK(intra, Reset)
            ERIS_SIM_NOTHR_WORK(intra, Optimize)
            ERIS_SIM_NOTHR_WORK(intra, Apply)
            ERIS_SIM_NOTHR_WORK(intra, Finish)
#undef ERIS_SIM_NOTHR_WORK
            case RunStage::intra_Reoptimize:
                thr_work<intraopt::Reoptimize>([this](intraopt::Reoptimize &opt) {
                    if (opt.intraReoptimize()) // Need a restart
                        thr_redo_intra_ = true;
                });
                break;
            default:
                break;
        }
    }
    else {
        // Threads: lock, signal, then wait for threads to finish
        std::unique_lock<std::mutex> lock_s(stage_mutex_, std::defer_lock);
        std::unique_lock<std::mutex> lock_d(done_mutex_, std::defer_lock);
        std::lock(lock_s, lock_d);
        stage_ = stage;
        thr_running_ = thr_pool_.size();
        lock_s.unlock();
        thr_cv_stage_.notify_all();

        thr_cv_done_.wait(lock_d, [this] { return thr_running_ == 0; });
    }
}

void Simulation::thr_thread_pool() {
    if (stage_ != RunStage::idle)
        throw std::runtime_error("Cannot enlarge thread pool during non-idle run phase");

    if (thr_pool_.size() == maxThreads()) {
        // Nothing to see here.  Move along.
        return;
    }
    else if (thr_pool_.size() > maxThreads()) {
        // Too many threads in the pool, kill some off
        for (unsigned int excess = thr_pool_.size() - maxThreads(); excess > 0; excess--) {
            auto &thr = thr_pool_.back();
            thr_kill_ = thr.get_id();
            stage_ = RunStage::kill;
            thr_cv_stage_.notify_all();
            thr.join();
            thr_pool_.pop_back();
        }
        stage_ = RunStage::idle;
    }
    else {
        // The pool is smaller than the maximum; see if there is a benefit to creating more threads
        unsigned long max_opts = 0;
        for (const auto &opt : optimizers_) {
            auto size = opt.size();
            if (size > max_opts)
                max_opts = size;
        }
        unsigned long want_threads = std::min(maxThreads(), max_opts);

        while (thr_pool_.size() < want_threads) {
            // We're in RunStage::idle, so the new thread is going to wait right away
            thr_pool_.push_back(std::thread(&Simulation::thr_loop, this));
        }
    }
}


std::unique_lock<std::mutex> Simulation::runLock() {
    return std::unique_lock<std::mutex>(run_mutex_);
}

void Simulation::run() {
    if (running_)
        throw std::runtime_error("Calling Simulation::run() recursively not permitted");
    auto lock = runLock();
    running_ = true;

    stage_ = RunStage::idle;

    // Enlarge or shrink the thread pool as needed
    thr_thread_pool();

    if (++iteration_ > 1) {
        // Skip all this on the first iteration
        thr_stage(RunStage::inter_Optimize);
        thr_stage(RunStage::inter_Apply);
        thr_stage(RunStage::inter_Advance);
    }

    intraopt_count = 0;

    thr_stage(RunStage::intra_Initialize);

    thr_redo_intra_ = true;
    while (thr_redo_intra_) {
        intraopt_count++;
        thr_stage(RunStage::intra_Reset);
        thr_stage(RunStage::intra_Optimize);
        thr_redo_intra_ = false;
        thr_stage(RunStage::intra_Reoptimize);
    }

    thr_stage(RunStage::intra_Apply);
    thr_stage(RunStage::intra_Finish);

    stage_ = RunStage::idle;
    running_ = false;
}

RunStage Simulation::runStage() const {
    return stage_;
}

Simulation::~Simulation() {
    stage_ = RunStage::kill_all;
    thr_cv_stage_.notify_all();

    for (auto &thr : thr_pool_) {
        thr.join();
    }
}

}

// vim:tw=100
