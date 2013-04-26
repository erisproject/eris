#include "Eris.hpp"
#include <iostream>

using namespace std;

int main() {
    Eris sim;
    for (int i = 0; i < 10; i++) {
        Agent a;
        sim.addAgent(a);
    }

    for (int i = 0; i < 3; i++) {
        Good g(0);
        sim.addGood(g);
    }

    for (auto agit = sim.agents(); agit != sim.agentsEnd(); ++agit) {
        Agent ag = agit->second;
        cout << "Agent: " << agit->first << " => " << ag.id() << "\n";
    }
    for (auto gdit = sim.goods(); gdit != sim.goodsEnd(); ++gdit) {
        Good gd = gdit->second;
        cout << "Good: id=" << gd.id() << ", name=" << gd.name() << "\n";
    }
}
