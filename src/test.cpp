#include "Eris.hpp"
#include "Simulation.hpp"
#include <iostream>

using namespace std;

int main() {
    Eris<Simulation> sim;
    for (int i = 0; i < 10; i++) {
        sim->addAgent(new Agent);
    }

    for (int i = 0; i < 3; i++) {
        sim->addGood(new Good(0));
    }

    for (auto agit = sim->agents(); agit != sim->agentsEnd(); ++agit) {
        auto agent = agit->second;
        cout << "Agent: " << agit->first << ", ptr: " << (long) agent.get() << "\n";
    }
    for (auto gdit = sim->goods(); gdit != sim->goodsEnd(); ++gdit) {
        auto good = gdit->second;
        cout << "Good: id=" << gdit->first << ", name=" << good->name() << ", ptr=" << (long) good.get() << "\n";
    }
}
