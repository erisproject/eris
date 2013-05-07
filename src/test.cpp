#include "Eris.hpp"
#include "Simulation.hpp"
#include "Agent.hpp"
#include "consumer/Quadratic.hpp"
#include <iostream>

using namespace std;
using namespace eris;
using namespace eris::consumer;

int main() {
    Eris<Simulation> sim;
    for (int i = 0; i < 10; i++) {
        sim->addAgent(Quadratic(1.0, {}));
    }

    for (int i = 0; i < 2; i++) {
        sim->addGood(Good::Continuous("continuous good"));
    }
    sim->addGood(Good::Discrete("discrete good"));

    for (auto a : sim->agents()) {
        auto agent = a.second;
        cout << "================== Agent: " << a.first << ", ptr: " << agent.ptr.get() << ", refcount: " << agent.ptr.use_count() << "\n";
    }
    for (auto g : sim->goods()) {
        auto good = g.second;
        cout << "================== Good: id=" << g.first << ", name=" << good->name << ", increment=" << good->increment() <<
            ", ptr=" << good.ptr.get() << ", refcount: " << good.ptr.use_count() << "\n";
    }


    // We should be able to automatically cast from a SharedObject<A> to a SharedObject<B> (assuming
    // that A can be cast as B):
    SharedMember<Quadratic> joeQ = sim->agent(1);
    {
        SharedMember<Agent> joeA = joeQ;
        std::cout << "joe has " << joeQ.ptr.use_count() << " referencees\n";
    }
    std::cout << "joe has " << joeQ.ptr.use_count() << " referencees\n";
//    SharedObject<Quadratic> joeQ = joe;
}
