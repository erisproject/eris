/// Simple example of a quantity-setting monopolist

#include <eris/Eris.hpp>
#include <eris/Simulation.hpp>
#include <eris/firm/QFirm.hpp>
#include <eris/market/Quantity.hpp>
#include <eris/interopt/QMStepper.hpp>
#include <eris/interopt/QFStepper.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/intraopt/MUPD.hpp>
#include <eris/interopt/FixedIncome.hpp>
#include <iostream>
#include <list>

using namespace eris;
using namespace eris::firm;
using namespace eris::consumer;
using namespace eris::market;
using namespace eris::interopt;
using namespace eris::intraopt;

int main() {

    Eris<Simulation> sim;

    auto m = sim->createGood<Good::Continuous>("money");
    auto x = sim->createGood<Good::Continuous>("x");

    auto m1 = Bundle(m, 1);
    auto x1 = Bundle(x, 1);

    // Set up a quantity-setting firm that produces x, with initial quantity of 100,
    // and complete depreciation.
    auto firm = sim->createAgent<QFirm>(x1, 100);
    sim->createInterOpt<QFStepper>(firm);

    auto qmkt = sim->createMarket<Quantity>(x1, m1);
    qmkt->addFirm(firm);

    std::list<SharedMember<Quadratic>> consumers;

    // Set up some agents, from 1 to 100, which agent j having utility m + x - x^2/(2j).
    // This is simple enough: the optimal price is 0.5, with agent j buying j/2 units.
    for (int j = 1; j <= 5; j++) {
        auto c = sim->createAgent<Quadratic>();
        c->coef(m) = 1;
        c->coef(x) = 1;
        c->coef(x, x) = -0.5 / j;
        consumers.push_back(c);

        // Use MUPD for optimization
        sim->createIntraOpt<MUPD>(c, m);

        // Give them some income:
        c->assets() += 100*m1;
        sim->createInterOpt<FixedIncome>(c, 100*m1);
    }

    for (int i = 0; i < 100; i++) {
        sim->run();

        std::cout << "Ran iteration " << i << ".  (" << sim->intraopt_count << " intraopt loops)\n";

        std::cout << "    P[x]: " << qmkt->price() << "\n";

        double Q = 0;
        for (auto c : consumers) {
            Q += c->assets()[x];
        }
        std::cout << "    Profit: " << Q*qmkt->price() << ", direct: " << firm->assets()[m] << "\n";
        std::cout << "    Capacity: " << firm->capacity << "\n";
        std::cout << "    Q[x]: " << Q << "\n";
        for (auto c: consumers) {
            std::cout << "    q[" << c->id() << "] = " << c->assets()[x] << "\n";
        }
    }

}
