#include <eris/Simulation.hpp>
#include <eris/SharedMember.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <eris/Market.hpp>
#include <eris/Optimize.hpp>
#include <algorithm>
#include <utility>

namespace eris {

std::shared_ptr<Simulation> Simulation::create() {
    return std::shared_ptr<Simulation>(new Simulation);
}

void Simulation::registerDependency(eris_id_t member, eris_id_t depends_on) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    depends_on_[depends_on].insert(member);
}

void Simulation::registerWeakDependency(eris_id_t member, eris_id_t depends_on) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    weak_dep_[depends_on].insert(member);
}

SharedMember<Member> Simulation::add(std::shared_ptr<Member> new_member) {
    SharedMember<Member> member(std::move(new_member));
    if (auto lock = runLockTry()) {
        insert(member);
    }
    else {
        deferred_mutex_.lock();
        deferred_insert_.push_back(member);
        deferred_mutex_.unlock();
    }
    return member;
}

void Simulation::insert(const SharedMember<Member> &member) {
    if (member->hasSimulation()) throw std::logic_error("Cannot insert member in a simulation multiple times");
    if (dynamic_cast<Agent*>(member.get())) insertAgent(member);
    else if (dynamic_cast<Good*>(member.get())) insertGood(member);
    else if (dynamic_cast<Market*>(member.get())) insertMarket(member);
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
    eris_id_t member_id = id_next_++;\
    MAP.insert(std::make_pair(member_id, member));\
    invalidateCache<CLASS>();\
    member->simulation(shared_from_this(), member_id);\
    insertOptimizers(member);\
}\
void Simulation::remove##TYPE(eris_id_t id) {\
    std::lock_guard<std::recursive_mutex> mbr_lock(member_mutex_);\
    auto member = MAP.at(id);\
    auto lock = member->writeLock();\
    removeOptimizers(member);\
    MAP.erase(id);\
    invalidateCache<CLASS>();\
    member->simulation(nullptr, 0);\
    removeDeps(id);\
    notifyWeakDeps(member, id);\
}
ERIS_SIM_INSERT_REMOVE_MEMBER(Agent,  Agent,  agents_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Good,   Good,   goods_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Market, Market, markets_)
ERIS_SIM_INSERT_REMOVE_MEMBER(Other,  Member, others_)
#undef ERIS_SIM_INSERT_REMOVE_MEMBER

// More searching help: these are in eris/Simulation.hpp:
// agent() agents() good() goods() market() markets() other() others()

void Simulation::remove(eris_id_t id) {
    if (auto lock = runLockTry())
        removeNoDefer(id);
    else {
        deferred_mutex_.lock();
        deferred_remove_.push_back(id);
        deferred_mutex_.unlock();
    }
}

void Simulation::removeNoDefer(eris_id_t id) {
    if (agents_.count(id)) removeAgent(id);
    else if (goods_.count(id)) removeGood(id);
    else if (markets_.count(id)) removeMarket(id);
    else if (others_.count(id)) removeOther(id);
    else throw std::out_of_range("eris_id_t to be removed does not exist");
}

void Simulation::processDeferredQueue() {
    // We need to take care to ensure that the mutex isn't locked when doing the actual
    // insertion because the insertion may itself trigger other insertions, which might need
    // to be deferred--but if the mutex is locked here, that will deadlock.  So we pull off
    // one insertion/removal at a time, release the lock, perform the insert/removal, then
    // reestablish the lock.
    deferred_mutex_.lock();
    while (not deferred_insert_.empty() or not deferred_remove_.empty()) {
        if (not deferred_insert_.empty()) {
            SharedMember<Member> to_insert = deferred_insert_.front();
            deferred_insert_.pop_front();
            deferred_mutex_.unlock();
            insert(to_insert);
        }
        else if (not deferred_remove_.empty()) {
            eris_id_t to_remove = deferred_remove_.front();
            deferred_remove_.pop_front();
            deferred_mutex_.unlock();
            removeNoDefer(to_remove);
        }
        deferred_mutex_.lock();
    }
    deferred_mutex_.unlock();
}


void Simulation::insertOptimizers(const SharedMember<Member> &member) {
    std::lock_guard<std::recursive_mutex> lock(member_mutex_);
    Member *mem = member.get();
#define ERIS_SIM_INSERT_OPTIMIZER(TYPE, STAGE)\
    if (TYPE##opt::STAGE *opt = dynamic_cast<TYPE##opt::STAGE*>(mem)) {\
        optimizers_[(int) RunStage::TYPE##_##STAGE][opt->TYPE##STAGE##Priority()].insert(member);\
    }
    ERIS_SIM_INSERT_OPTIMIZER(inter, Begin)
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
    for (auto &stage : optimizers_) {
        std::list<double> del_stage;
        for (auto &priority : stage) {
            if (priority.second.erase(member)) {
                // Removed it; trigger a plurality recalculation if we were the largest (we can't
                // just decrement it because something else might have had the same plurality).
                if (optimizers_plurality_ == (long) priority.second.size() + 1) {
                    optimizers_plurality_ = -1;
                }
            }
            // Clean up: if we deleted the last element at this priority level, delete the priority
            // level (but we need to wait until the end of the loop to avoid invalidating iterators)
            if (priority.second.empty()) del_stage.push_back(priority.first);
        }
        for (double priority : del_stage) {
            stage.erase(priority);
        }
    }
}

void Simulation::removeDeps(eris_id_t member) {
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

void Simulation::notifyWeakDeps(SharedMember<Member> member, eris_id_t old_id) {
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
    if (auto lock = runLockTry())
        max_threads_ = max_threads;
    else
        throw std::runtime_error("Cannot change number of threads during a Simulation run() call");
}

inline void Simulation::thr_stage_finished(const RunStage &curr_stage, double curr_priority) {
    {
        std::unique_lock<std::mutex> lock(done_mutex_);
        if (--thr_running_ == 0)
            thr_cv_done_.notify_one();
    }
    thr_wait(curr_stage, curr_priority);
}

inline void Simulation::thr_wait(const RunStage &not_stage, double not_priority) {
    std::unique_lock<std::mutex> lock(stage_mutex_);
    if (stage_ == not_stage and stage_priority_ == not_priority)
        thr_cv_stage_.wait(lock, [this,&not_stage,&not_priority] { return stage_ != not_stage or stage_priority_ != not_priority; });
}

void Simulation::thr_loop() {
    // Continually looks for the various stages and responds according.  Threads are allowed to
    // start work in any stage except idle.  The thread continues forever unless it sees a kill
    // stage with thr_kill_ set to its thread id.
    while (true) {
        stage_mutex_.lock();
        RunStage curr_stage = stage_;
        double curr_priority = stage_priority_;
        stage_mutex_.unlock();
        switch (curr_stage) {
            // Every case should either exit the loop or wait
            case RunStage::idle:
                // Just wait for the state to change to something else.
                thr_wait(curr_stage, curr_priority);
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
                thr_stage_finished(curr_stage, curr_priority);\
                break

            // Inter-period optimizer stages
            ERIS_SIM_STAGE_CASE(inter, Begin);
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
                thr_stage_finished(curr_stage, curr_priority);
                break;
        }
    }
}

template <class Opt>
void Simulation::thr_work(const std::function<void(Opt&)> &work) {
    if (maxThreads() > 0) {
        // get a lock before we check opt_ierator_
        opt_iterator_mutex_.lock();
        while (opt_iterator_ != opt_iterator_end_) {
            // We're not at the end, so we pull off the current member:
            Opt &opt = dynamic_cast<Opt&>(**opt_iterator_);
            // and then increment the iterator for the next thread (possibly us, if we finish this
            // optimizer quickly enough, or there are no other threads)
            opt_iterator_++;
            // Release the lock so any waiting thread can grab the next optimizer while we do work
            opt_iterator_mutex_.unlock();
            // Run the optimizer:
            work(opt);
            // Done working; re-establish the lock before checking if we're at the end
            opt_iterator_mutex_.lock();
        }
        // We hit the end; release the lock and return.
        opt_iterator_mutex_.unlock();
    }
    else {
        // No threads: same as above but without locking
        while (opt_iterator_ != opt_iterator_end_) {
            work(dynamic_cast<Opt&>(*(*opt_iterator_++)));
        }
    }
}

void Simulation::thr_stage(const RunStage &stage) {
    if (stage < RunStage_FIRST)
        throw std::runtime_error("thr_stage called with non-stage RunStage");

    if (maxThreads() == 0) {
        // Not using threads; call thr_work directly
        stage_ = stage;
        for (auto &priority_optimizer : optimizers_[(int) stage]) {
            stage_priority_   = priority_optimizer.first;
            opt_iterator_     = priority_optimizer.second.begin();
            opt_iterator_end_ = priority_optimizer.second.end();
            switch (stage) {
#define ERIS_SIM_NOTHR_WORK(TYPE, STAGE)\
                case RunStage::TYPE##_##STAGE:\
                    thr_work<TYPE##opt::STAGE>([](TYPE##opt::STAGE &o) { o.TYPE##STAGE(); });\
                    break;
                ERIS_SIM_NOTHR_WORK(inter, Begin)
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
                case RunStage::idle:
                case RunStage::kill:
                case RunStage::kill_all:
                    break;
            }

            // Deferred insertion/removal: this could invalidate opt_iterator_
            processDeferredQueue();
        }
    }
    else {
        for (auto &priority_optimizer : optimizers_[(int) stage]) {
            // Threads: lock, signal, then wait for threads to finish
            std::unique_lock<std::mutex> lock_s(stage_mutex_, std::defer_lock);
            std::unique_lock<std::mutex> lock_d(done_mutex_, std::defer_lock);
            std::unique_lock<std::mutex> lock_i(opt_iterator_mutex_, std::defer_lock);
            std::lock(lock_s, lock_d, lock_i);
            stage_ = stage;
            stage_priority_   = priority_optimizer.first;
            opt_iterator_     = priority_optimizer.second.begin();
            opt_iterator_end_ = priority_optimizer.second.end();

            thr_running_ = thr_pool_.size();
            lock_i.unlock();
            lock_s.unlock();
            thr_cv_stage_.notify_all();

            thr_cv_done_.wait(lock_d, [this] { return thr_running_ == 0; });

            // The stage is done; handle deferred insertion/removal
            processDeferredQueue();
        }
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
            // Wait for the thread we just signalled to exit:
            thr.join();
            thr_pool_.pop_back();
        }
        stage_ = RunStage::idle;
    }
    else {
        // The pool is smaller than the maximum; see if there is a benefit to creating more threads
        if (optimizers_plurality_ < 0) {
            optimizers_plurality_ = 0;
            for (const auto &stage : optimizers_) {
                for (const auto &priority : stage) {
                    optimizers_plurality_ = std::max(optimizers_plurality_, (long) priority.second.size());
                }
            }
        }
        unsigned long want_threads = std::min(maxThreads(), (unsigned long) optimizers_plurality_);

        while (thr_pool_.size() < want_threads) {
            // We're in RunStage::idle *and* the stage is locked, so the new thread is going to block right away
            thr_pool_.push_back(std::thread(&Simulation::thr_loop, this));
        }
    }
}

boost::shared_lock<boost::shared_mutex> Simulation::runLock() {
    return boost::shared_lock<boost::shared_mutex>(run_mutex_);
}

boost::shared_lock<boost::shared_mutex> Simulation::runLockTry() {
    return boost::shared_lock<boost::shared_mutex>(run_mutex_, boost::try_to_lock);
}

void Simulation::run() {
    boost::unique_lock<boost::shared_mutex> lock(run_mutex_);

    stage_mutex_.lock();
    stage_ = RunStage::idle;
    stage_priority_ = 0;

    // Enlarge or shrink the thread pool as needed
    thr_thread_pool();

    ++t_;

    stage_mutex_.unlock();
    // We're in idle right now, so any threads that got unblocked here will go right back to sleep

    thr_stage(RunStage::inter_Begin);
    thr_stage(RunStage::inter_Optimize);
    thr_stage(RunStage::inter_Apply);
    thr_stage(RunStage::inter_Advance);

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
}

Simulation::RunStage Simulation::runStage() const {
    return stage_;
}

bool Simulation::runStageIntra() const {
    switch ((RunStage) stage_) {
        case RunStage::intra_Initialize:
        case RunStage::intra_Reset:
        case RunStage::intra_Optimize:
        case RunStage::intra_Reoptimize:
        case RunStage::intra_Apply:
        case RunStage::intra_Finish:
            return true;
        default:
            return false;
    }
}

bool Simulation::runStageInter() const {
    switch ((RunStage) stage_) {
        case RunStage::inter_Begin:
        case RunStage::inter_Optimize:
        case RunStage::inter_Apply:
        case RunStage::inter_Advance:
            return true;
        default:
            return false;
    }
}

bool Simulation::runStageOptimize() const {
    switch ((RunStage) stage_) {
        case RunStage::inter_Optimize:
        case RunStage::intra_Reset:
        case RunStage::intra_Optimize:
        case RunStage::intra_Reoptimize:
            return true;
        default:
            return false;
    }
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
