// Test script for assorted Simulation setup tasks.

#include <eris/Bundle.hpp>
#include <eris/Simulation.hpp>
#include <eris/consumer/Polynomial.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/consumer/Compound.hpp>
#include <eris/consumer/CobbDouglas.hpp>
#include <eris/intraopt/MUPD.hpp>
#include <eris/market/Bertrand.hpp>
#include <eris/Good.hpp>
#include <cmath>
#include <gtest/gtest.h>
#include <sstream>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using namespace eris::intraopt;

std::string as_string(const unordered_map<eris::id_t, unordered_set<eris::id_t>> &deps) {
    // Stick everything into a map/set, so that it is ordered
    map<eris::id_t, set<eris::id_t>> m;
    for (auto d : deps) {
        m[d.first] = set<eris::id_t>(d.second.cbegin(), d.second.cend());
    }

    stringstream s;
    bool first = true;
    for (auto d : m) {
        if (first) first = false;
        else s << ", ";

        s << "[" << d.first << "]={";
        bool first2 = true;
        for (auto a : d.second) {
            if (first2) first2 = false;
            else s << ",";
            s << a;
        }
        s << "}";
    }

    return s.str();
}

#define MEMBERS(ID, type, Type) \
    sim->type##s<Type>([&](const Type &m) { return m.id() == ID; })
#define AGENTS(id) MEMBERS(id, agent, Agent)
#define GOODS(id) MEMBERS(id, good, Good)
#define OTHERS(id) MEMBERS(id, other, Member)

TEST(Dependencies, Create) {

    auto sim = Simulation::create();

    // Create some goods and agents
    auto m = sim->spawn<Good>("Money");
    auto x = sim->spawn<Good>("x");
    auto y = sim->spawn<Good>("y");

    auto con = sim->spawn<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    // Declare some dependencies
    sim->registerDependency(x, y);
    sim->registerDependency(con, y);
    sim->registerDependency(y, x);
    sim->registerDependency(x, m);

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->spawn<MUPD>(con, m);

    // Store these as they will become 0 when the members are removed
    auto mid = m->id();
    auto xid = x->id();
    auto yid = y->id();
    auto cid = con->id();
    auto oid = opt->id();

    Simulation::DepMap want = {
        { cid, { oid } },
        { mid, { oid, xid } },
        { xid, { yid } },
        { yid, { cid, xid } },
    };

    EXPECT_EQ(as_string(want), as_string(sim->__deps()));

    EXPECT_EQ(xid, x->id());
    EXPECT_EQ(yid, y->id());
    EXPECT_EQ(1u, GOODS(mid).size());
    EXPECT_EQ(1u, GOODS(xid).size());
    EXPECT_EQ(1u, GOODS(yid).size());
    EXPECT_EQ(1u, AGENTS(cid).size());
    EXPECT_EQ(1u, OTHERS(oid).size());
}

TEST(Dependencies, Delete) {

    auto sim = Simulation::create();

    // Create some goods and agents
    auto m = sim->spawn<Good>("Money");
    auto x = sim->spawn<Good>("x");
    auto y = sim->spawn<Good>("y");

    auto con = sim->spawn<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    sim->registerDependency(y, x);

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->spawn<MUPD>(con, m);

    // Store these as they will become 0 when the members are removed
    auto mid = m->id();
    auto xid = x->id();
    auto yid = y->id();
    auto cid = con->id();
    auto oid = opt->id();

    sim->remove(x);

    Simulation::DepMap want = {
        { cid, { oid } },
        { mid, { oid } },
//        { xid, { yid } }
    };

    EXPECT_EQ(as_string(want), as_string(sim->__deps()));

    EXPECT_FALSE(x->hasSimulation());
    EXPECT_FALSE(y->hasSimulation());
    EXPECT_EQ(0u, GOODS(xid).size());
    EXPECT_EQ(0u, GOODS(yid).size());
    EXPECT_EQ(1u, GOODS(mid).size());
    EXPECT_EQ(cid, sim->agent(cid)->id());
    EXPECT_EQ(oid, sim->other(oid)->id());

    sim->remove(m);

    want = {
        { cid, { oid } }, // Stale, but okay.
//        { mid, { oid } },
//        { xid, { yid } }
    };

    EXPECT_EQ(as_string(want), as_string(sim->__deps()));

    EXPECT_FALSE(x->hasSimulation());
    EXPECT_FALSE(y->hasSimulation());
    EXPECT_FALSE(m->hasSimulation());
    EXPECT_EQ(0u, GOODS(xid).size());
    EXPECT_EQ(0u, GOODS(yid).size());
    EXPECT_EQ(0u, GOODS(mid).size());
    EXPECT_EQ(1u, AGENTS(cid).size());
    EXPECT_EQ(0u, OTHERS(oid).size());
}

TEST(Dependencies, DeleteChain) {

    auto sim = Simulation::create();

    // Create some goods and agents
    auto m = sim->spawn<Good>("Money");
    auto x = sim->spawn<Good>("x");
    auto y = sim->spawn<Good>("y");

    auto con = sim->spawn<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->spawn<MUPD>(con, m);

    // Declare some dependencies
    sim->registerDependency(x, y);
    sim->registerDependency(con, y);
    sim->registerDependency(y, x);
    sim->registerDependency(x, m);

    // Store these as they will become 0 when the members are removed
    auto mid = m->id();
    auto xid = x->id();
    auto yid = y->id();
    auto cid = con->id();
    auto oid = opt->id();

    // Everything now depends (directly or indirectly) on m, so let's delete it:
    sim->remove(m);

    EXPECT_EQ("", as_string(sim->__deps()));

    EXPECT_FALSE(x->hasSimulation());
    EXPECT_FALSE(y->hasSimulation());
    EXPECT_FALSE(m->hasSimulation());
    EXPECT_FALSE(con->hasSimulation());
    EXPECT_FALSE(opt->hasSimulation());
    EXPECT_EQ(0u, GOODS(xid).size());
    EXPECT_EQ(0u, GOODS(yid).size());
    EXPECT_EQ(0u, GOODS(mid).size());
    EXPECT_EQ(0u, sim->goods().size());
    EXPECT_EQ(0u, AGENTS(cid).size());
    EXPECT_EQ(0u, sim->agents().size());
    EXPECT_EQ(0u, OTHERS(oid).size());
    EXPECT_EQ(0u, sim->others().size());

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
