// Test script for assorted Simulation setup tasks.

#include <eris/Bundle.hpp>
#include <eris/Simulation.hpp>
#include <eris/consumer/Polynomial.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/consumer/Compound.hpp>
#include <eris/consumer/CobbDouglas.hpp>
#include <eris/intraopt/MUPD.hpp>
#include <eris/market/Bertrand.hpp>
#include <cmath>
#include <gtest/gtest.h>
#include <sstream>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using namespace eris::intraopt;

std::string as_string(const unordered_map<eris_id_t, unordered_set<eris_id_t>> &deps) {
    // Stick everything into a map/set, so that it is ordered
    map<eris_id_t, set<eris_id_t>> m;
    for (auto d : deps) {
        m[d.first] = set<eris_id_t>(d.second.cbegin(), d.second.cend());
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

#define MEMBERS(id, type, Type) \
    sim->type##s<Type>([&](const Type &m) { return m == id; })
#define AGENTS(id) MEMBERS(id, agent, Agent)
#define GOODS(id) MEMBERS(id, good, Good)
#define OTHERS(id) MEMBERS(id, other, Member)

TEST(Dependencies, Create) {

    auto sim = Simulation::spawn();

    // Create some goods and agents
    auto m = sim->create<Good::Continuous>("Money");
    auto x = sim->create<Good::Continuous>("x");
    auto y = sim->create<Good::Continuous>("y");

    auto con = sim->create<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    // Declare some dependencies
    sim->registerDependency(x, y);
    sim->registerDependency(con, y);
    sim->registerDependency(y, x);
    sim->registerDependency(x, m);

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->create<MUPD>(con, m);

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
    EXPECT_EQ(1, GOODS(mid).size());
    EXPECT_EQ(1, GOODS(xid).size());
    EXPECT_EQ(1, GOODS(yid).size());
    EXPECT_EQ(1, AGENTS(cid).size());
    EXPECT_EQ(1, OTHERS(oid).size());
}

TEST(Dependencies, Delete) {

    auto sim = Simulation::spawn();

    // Create some goods and agents
    auto m = sim->create<Good::Continuous>("Money");
    auto x = sim->create<Good::Continuous>("x");
    auto y = sim->create<Good::Continuous>("y");

    auto con = sim->create<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    sim->registerDependency(y, x);

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->create<MUPD>(con, m);

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

    EXPECT_EQ(0, x->id());
    EXPECT_EQ(0, y->id());
    EXPECT_EQ(0, GOODS(xid).size());
    EXPECT_EQ(0, GOODS(yid).size());
    EXPECT_EQ(1, GOODS(mid).size());
    EXPECT_EQ(cid, sim->agent(cid));
    EXPECT_EQ(oid, sim->other(oid));

    sim->remove(m);

    want = {
        { cid, { oid } }, // Stale, but okay.
//        { mid, { oid } },
//        { xid, { yid } }
    };

    EXPECT_EQ(as_string(want), as_string(sim->__deps()));

    EXPECT_EQ(0, x->id());
    EXPECT_EQ(0, y->id());
    EXPECT_EQ(0, m->id());
    EXPECT_EQ(0, GOODS(xid).size());
    EXPECT_EQ(0, GOODS(yid).size());
    EXPECT_EQ(0, GOODS(mid).size());
    EXPECT_EQ(1, AGENTS(cid).size());
    EXPECT_EQ(0, OTHERS(oid).size());
}

TEST(Dependencies, DeleteChain) {

    auto sim = Simulation::spawn();

    // Create some goods and agents
    auto m = sim->create<Good::Continuous>("Money");
    auto x = sim->create<Good::Continuous>("x");
    auto y = sim->create<Good::Continuous>("y");

    auto con = sim->create<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    // a MUPD optimizer should declare a dependency on both the consumer and
    // the money good:
    auto opt = sim->create<MUPD>(con, m);

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

    EXPECT_EQ(0, x->id());
    EXPECT_EQ(0, y->id());
    EXPECT_EQ(0, m->id());
    EXPECT_EQ(0, con->id());
    EXPECT_EQ(0, opt->id());
    EXPECT_EQ(0, GOODS(xid).size());
    EXPECT_EQ(0, GOODS(yid).size());
    EXPECT_EQ(0, GOODS(mid).size());
    EXPECT_EQ(0, sim->goods().size());
    EXPECT_EQ(0, AGENTS(cid).size());
    EXPECT_EQ(0, sim->agents().size());
    EXPECT_EQ(0, OTHERS(oid).size());
    EXPECT_EQ(0, sim->others().size());

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
