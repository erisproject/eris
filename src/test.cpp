#include "Eris.hpp"
#include "Simulation.hpp"
#include "Agent.hpp"
#include "consumer/Quadratic.hpp"
#include "consumer/Consumer.hpp"
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
}
