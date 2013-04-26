#include "Eris.hpp"
#include "agent/consumer/Quadratic.hpp"
#include <iostream>

using namespace std;

int main() {
    Eris sim;
    Agent a;
    sim.addAgent(a);

    // Set up a numeraire good
    eris_id_t money = sim.addGood(Good("Money", 0));
    // Plus another divisible good
    eris_id_t good = sim.addGood(Good("x", 0));


    // We have just a single consumer, with quaslinear quadratic utility in the x good
    Quadratic fred;
    fred.setCoefs(money, {1});
    fred.setCoefs(good, {5, -1});

    Bundle b;
    for (int m = 0; m <= 10; m++) {
        b[money] = 10-m;
        b[good] = m;

        cout << "Fred's utility of n="<<10-m<<", x="<<m<<": " << fred.utility(b) << "\n";
    }

    /*for (int i = 0; i < 3; i++) {
        sim.addGood(&g);
    }

    for (auto agit = sim.agents(); agit != sim.agentsEnd(); ++agit) {
        Agent ag = agit->second;
        cout << "Agent: " << agit->first << " => " << ag.id() << "\n";
    }
    for (auto gdit = sim.goods(); gdit != sim.goodsEnd(); ++gdit) {
        Good gd = gdit->second;
        cout << "Good: " << gdit->first << " => " << gd.id() << "\n";
    }*/
}
