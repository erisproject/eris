// Test script for testing simulation stage and priority ordering

#include <eris/Simulation.hpp>
#include <eris/Member.hpp>
#include <eris/Optimize.hpp>
#include <eris/intraopt/Callback.hpp>
#include <eris/interopt/Callback.hpp>
#include <eris/random/rng.hpp>
#include <eris/random/util.hpp>
#include <cmath>
#include <set>
#include <gtest/gtest.h>
#include <sstream>
#include <mutex>
#include <algorithm>

using namespace std;
using namespace eris;


// Basic idea: start master_value at 0, then expect it to be set by various optimizers to 1, then 2,
// then 3, etc.  Any attempts to decrease the value (e.g. 3 -> 2) or to increment by more than 1
// (e.g. 2 -> 4) are errors that get stored in master_fails.
int master_value;
std::set<std::pair<int, int>> master_fails; // invalid {from, to} pairs
std::mutex master_mutex;
void reset_master() {
    master_mutex.lock();
    master_value = 0;
    master_fails.clear();
    master_mutex.unlock();
}
void set_master(int value) {
    master_mutex.lock();
    if (value != master_value and value != master_value+1)
        master_fails.emplace(master_value, value);
    master_value = value;
    master_mutex.unlock();
}
int get_master() {
    master_mutex.lock();
    int mv = master_value;
    master_mutex.unlock();
    return mv;
}

#define OPTIMIZER_CLASS_W_RETURN(TYPE, type, STAGE, RETURN_TYPE, RETURN) \
class TYPE##STAGE##Test : public virtual type##opt::STAGE, public Member { \
    public: \
        TYPE##STAGE##Test() = delete; \
        TYPE##STAGE##Test(double priority, int value) : pri_{priority}, val_{value} {} \
        TYPE##STAGE##Test(const std::pair<double, int> &pair) : pri_{pair.first}, val_{pair.second} {} \
        virtual double type##STAGE##Priority() const override { return pri_; } \
        virtual RETURN_TYPE type##STAGE() override { set_master(val_); RETURN; } \
    private: \
        double pri_; \
        int val_; \
}
#define OPTIMIZER_CLASS(TYPE, type, STAGE) OPTIMIZER_CLASS_W_RETURN(TYPE, type, STAGE, void, return)
OPTIMIZER_CLASS(Intra, intra, Initialize);
OPTIMIZER_CLASS(Intra, intra, Reset);
OPTIMIZER_CLASS(Intra, intra, Optimize);
OPTIMIZER_CLASS_W_RETURN(Intra, intra, Reoptimize, bool, return false);
OPTIMIZER_CLASS(Intra, intra, Apply);
OPTIMIZER_CLASS(Intra, intra, Finish);

OPTIMIZER_CLASS(Inter, inter, Begin);
OPTIMIZER_CLASS(Inter, inter, Optimize);
OPTIMIZER_CLASS(Inter, inter, Apply);
OPTIMIZER_CLASS(Inter, inter, Advance);

enum class OC { IntraInitialize, IntraReset, IntraOptimize, IntraReoptimize, IntraApply, IntraFinish,
    InterBegin, InterOptimize, InterApply, InterAdvance };

// First test that individual stages get called in the right order
TEST(Stage, Ordering) {

    auto sim = Simulation::create();
    sim->maxThreads(std::thread::hardware_concurrency());

    std::vector<OC> order;
    for (int i = 0; i < 100; i++) {
        order.push_back(OC::IntraInitialize);
        order.push_back(OC::IntraReset);
        order.push_back(OC::IntraOptimize);
        order.push_back(OC::IntraReoptimize);
        order.push_back(OC::IntraApply);
        order.push_back(OC::IntraFinish);
        order.push_back(OC::InterBegin);
        order.push_back(OC::InterOptimize);
        order.push_back(OC::InterApply);
        order.push_back(OC::InterAdvance);
    }

    // Shuffle the ordering so that we don't get sequential ordering that "looks" right only because
    // of the order we added into the simulation
    std::shuffle(order.begin(), order.end(), random::rng());

    for (auto oc : order) {
        switch (oc) {
#define CASE(Z, V) case OC::Z: sim->spawn<Z##Test>(0, V); break
            CASE(InterBegin, 1);
            CASE(InterOptimize, 2);
            CASE(InterApply, 3);
            CASE(InterAdvance, 4);
            CASE(IntraInitialize, 5);
            CASE(IntraReset, 6);
            CASE(IntraOptimize, 7);
            CASE(IntraReoptimize, 8);
            CASE(IntraApply, 9);
            CASE(IntraFinish, 10);
#undef CASE
        }
    }

    std::cerr << "AsdfASDF\n";
    reset_master();
    std::cerr << "AsdfASDF\n";
    sim->run();
    std::cerr << "AsdfASDF\n";

    ASSERT_TRUE(master_mutex.try_lock());
    master_mutex.unlock();
    std::cerr << "AsdfASDF " << __LINE__ << "\n";
    EXPECT_EQ(10, get_master());
    std::cerr << "AsdfASDF " << __LINE__ << "\n";
    EXPECT_TRUE(master_fails.empty());
    std::cerr << "AsdfASDF " << __LINE__ << "\n";
    for (auto &from_to : master_fails) ADD_FAILURE() << "Order failure: tried to change " << from_to.first << " to " << from_to.second;
    std::cerr << "AsdfASDF " << __LINE__ << "\n";
    std::cerr << "AsdfASDF " << __LINE__ << "\n";
}

// Same as above, but with random priorities on each stage
TEST(Stage, OrderingWithPriorities) {

    auto sim = Simulation::create();
    sim->maxThreads(std::thread::hardware_concurrency());

    std::vector<OC> order;
    for (int i = 0; i < 100; i++) {
        order.push_back(OC::IntraInitialize);
        order.push_back(OC::IntraReset);
        order.push_back(OC::IntraOptimize);
        order.push_back(OC::IntraReoptimize);
        order.push_back(OC::IntraApply);
        order.push_back(OC::IntraFinish);
        order.push_back(OC::InterBegin);
        order.push_back(OC::InterOptimize);
        order.push_back(OC::InterApply);
        order.push_back(OC::InterAdvance);
    }

    // Shuffle the ordering so that we don't get sequential ordering that "looks" right only because
    // of the order we added into the simulation
    std::shuffle(order.begin(), order.end(), random::rng());

    for (auto oc : order) {
        switch (oc) {
#define CASE(Z, V) case OC::Z: sim->spawn<Z##Test>(random::rnormal(), V); break
            CASE(InterBegin, 1);
            CASE(InterOptimize, 2);
            CASE(InterApply, 3);
            CASE(InterAdvance, 4);
            CASE(IntraInitialize, 5);
            CASE(IntraReset, 6);
            CASE(IntraOptimize, 7);
            CASE(IntraReoptimize, 8);
            CASE(IntraApply, 9);
            CASE(IntraFinish, 10);
#undef CASE
        }
    }

    reset_master();
    sim->run();

    ASSERT_TRUE(master_mutex.try_lock());
    master_mutex.unlock();
    EXPECT_EQ(10, get_master());
    EXPECT_TRUE(master_fails.empty());
    for (auto &from_to : master_fails) ADD_FAILURE() << "Order failure: tried to change " << from_to.first << " to " << from_to.second;
}

// Test that priority ordering within intraOptimize works
TEST(Priority, WithinStageOrdering) {

    auto sim = Simulation::create();

    std::vector<std::pair<double, int>> order;
    for (int i = 0; i < 100; i++) {
        order.emplace_back(-std::numeric_limits<double>::infinity(), 1);
        order.emplace_back(-100, 2);
        order.emplace_back(0, 3); // Will use default value (not 0)
        order.emplace_back(1e-100, 4);
        order.emplace_back(1, 5);
        order.emplace_back(2, 6);
        order.emplace_back(7e300, 7);
        order.emplace_back(std::numeric_limits<double>::infinity(), 8);
    }

    std::shuffle(order.begin(), order.end(), random::rng());

    for (auto &pair : order) {
        if (pair.first == 0) {
            // Don't use IntraOptimizeTest, install fall back on default priority (which should be 0)
            auto v = pair.second;
            sim->spawn<intraopt::OptimizeCallback>([v](){ set_master(v); });
        }
        else {
            sim->spawn<IntraOptimizeTest>(pair.first, pair.second);
        }
    }

    reset_master();
    sim->run();

    ASSERT_TRUE(master_mutex.try_lock());
    master_mutex.unlock();
    EXPECT_EQ(8, get_master());
    EXPECT_TRUE(master_fails.empty());
    for (auto &from_to : master_fails) ADD_FAILURE() << "Order failure: tried to change " << from_to.first << " to " << from_to.second;
}

// Test that mixing stages and priorities works as expected, i.e. order by stage first, priority
// second.
TEST(Priority, AcrossStageOrdering) {
    auto sim = Simulation::create();

    std::vector<std::pair<double, int>> order;
    for (int i = 0; i < 100; i++) {
        order.emplace_back(-std::numeric_limits<double>::infinity(), 1);
        order.emplace_back(-100, 2);
        order.emplace_back(0, 3); // Will use default value (not 0)
        order.emplace_back(1e-100, 4);
        order.emplace_back(1, 5);
        order.emplace_back(2, 6);
        order.emplace_back(7e300, 7);
        order.emplace_back(std::numeric_limits<double>::infinity(), 8);

        order.emplace_back(-std::numeric_limits<double>::infinity(), 9);
        order.emplace_back(-100, 10);
        order.emplace_back(0, 11); // Will use default value (not 0)
        order.emplace_back(1e-100, 12);
        order.emplace_back(1, 13);
        order.emplace_back(2, 14);
        order.emplace_back(7e300, 15);
        order.emplace_back(std::numeric_limits<double>::infinity(), 16);

        order.emplace_back(-std::numeric_limits<double>::infinity(), 17);
        order.emplace_back(-100, 18);
        order.emplace_back(0, 19); // Will use default value (not 0)
        order.emplace_back(1e-100, 20);
        order.emplace_back(1, 21);
        order.emplace_back(2, 22);
        order.emplace_back(7e300, 23);
        order.emplace_back(std::numeric_limits<double>::infinity(), 24);

        order.emplace_back(-std::numeric_limits<double>::infinity(), 25);
        order.emplace_back(-100, 26);
        order.emplace_back(0, 27); // Will use default value (not 0)
        order.emplace_back(1e-100, 28);
        order.emplace_back(1, 29);
        order.emplace_back(2, 30);
        order.emplace_back(7e300, 31);
        order.emplace_back(std::numeric_limits<double>::infinity(), 32);
    }

    std::shuffle(order.begin(), order.end(), random::rng());

    for (auto &pair : order) {
        if (pair.second <= 8) { // First block: interOptimize
            if (pair.first == 0) {
                auto v = pair.second;
                sim->spawn<interopt::OptimizeCallback>([v](){ set_master(v); });
            }
            else {
                sim->spawn<InterOptimizeTest>(pair);
            }
        }
        else if (pair.second <= 16) { // Second block: intraInitialize
            if (pair.first == 0) {
                auto v = pair.second;
                sim->spawn<intraopt::InitializeCallback>([v](){ set_master(v); });
            }
            else {
                sim->spawn<IntraInitializeTest>(pair);
            }
        }
        else if (pair.second <= 24) { // Third block: intraReoptimize
            if (pair.first == 0) {
                auto v = pair.second;
                sim->spawn<intraopt::ReoptimizeCallback>([v](){ set_master(v); return false; });
            }
            else {
                sim->spawn<IntraReoptimizeTest>(pair);
            }
        }
        else if (pair.second <= 32) { // Fourth block: intraFinish
            if (pair.first == 0) {
                auto v = pair.second;
                sim->spawn<intraopt::FinishCallback>([v](){ set_master(v); });
            }
            else {
                sim->spawn<IntraFinishTest>(pair);
            }
        }
        else {
            FAIL() << "Test error: found unexpected pair.second value";
        }
    }

    reset_master();
    sim->run();

    ASSERT_TRUE(master_mutex.try_lock());
    master_mutex.unlock();
    EXPECT_EQ(32, get_master());
    EXPECT_TRUE(master_fails.empty());
    for (auto &from_to : master_fails) ADD_FAILURE() << "Order failure: tried to change " << from_to.first << " to " << from_to.second;
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
