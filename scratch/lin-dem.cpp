#include <eris/Eris.hpp>
#include <eris/Simulation.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/firm/PriceFirm.hpp>
#include <iostream>
#include <map>
#include <boost/format.hpp>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using namespace eris::firm;
using boost::format;

int main() {
    Eris<Simulation> sim;

    // Set up a numeraire good
    auto money = sim->addGood(Good::Continuous("Money"));
    // Plus another divisible good
    auto x = sim->addGood(Good::Continuous("x"));
    // And a discrete good
    auto w = sim->addGood(Good::Discrete("w"));


    // We have just a single consumer, with quaslinear quadratic utility in the x good
    Quadratic c1(0.0, {{*money, 1}, {*x, 10}, {*w, 100}});

    c1.setQuadCoef(*money, *x, -1.1);
    c1.setQuadCoef(*money, *w, -0.1);
    c1.setQuadCoef(*x, *x, -1);
    c1.setQuadCoef(*w, *w, -1);
    c1.setQuadCoef(*x, *w, 0.3);

    sim->addAgent(c1);

    // And a price-setting firm:
    PriceFirm f1(Bundle(x, 1), Bundle(money, 2));

    /*
    c1[money] = {1};
    c1[x] = {5, -1};
    c1[w] = {2, -1};
    */

    /*for (int i = 0; i < 3; i++) {
        sim->addGood(&g);
    }

    for (auto agit = sim->agents(); agit != sim->agentsEnd(); ++agit) {
        Agent ag = agit->second;
        cout << "Agent: " << agit->first << " => " << ag.id() << "\n";
    }
    for (auto gdit = sim->goods(); gdit != sim->goodsEnd(); ++gdit) {
        Good gd = gdit->second;
        cout << "Good: " << gdit->first << " => " << gd.id() << "\n";
    }*/
}
