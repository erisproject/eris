#include "Eris.hpp"
#include "Simulation.hpp"
#include <iostream>

using namespace std;
using namespace eris;

int main() {
    Eris<Simulation> sim;
    for (int i = 0; i < 10; i++) {
        sim->addAgent(new Agent);
    }

    for (int i = 0; i < 2; i++) {
        sim->addGood(new Good::Continuous());
    }
    sim->addGood(new Good::Discrete());

    for (auto agit = sim->agents(); agit != sim->agentsEnd(); ++agit) {
        auto agent = agit->second;
        cout << "Agent: " << agit->first << ", ptr: " << (long) agent.get() << "\n";
    }
    for (auto gdit = sim->goods(); gdit != sim->goodsEnd(); ++gdit) {
        auto good = gdit->second;
        cout << "Good: id=" << gdit->first << ", name=" << good->name << ", increment=" << good->increment() << ", ptr=" << (long) good.get() << "\n";
    }
}
