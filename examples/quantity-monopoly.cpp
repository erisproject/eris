/// Simple example of a quantity-setting monopolist

#include <eris/Eris.hpp>
#include <eris/Simulation.hpp>
#include <eris/firm/QFirm.hpp>
#include <eris/market/QMarket.hpp>
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
    sim->createInterOpt<QFStepper>(firm, m1);

    auto qmkt = sim->createMarket<QMarket>(x1, m1, 1.0, 7);
    qmkt->addFirm(firm);

//    sim->createIntraOpt<WalrasianPricer>(qmkt, Stepper(Stepper::default_initial_step, Stepper::default_increase_count, 1e-6), 7);

    std::cout << "qmkt->optimizer=" << qmkt->optimizer << "\n";

    std::list<SharedMember<Quadratic>> consumers;

    // Set up some agents, from 1 to 100, which agent j having utility m + x - x^2/(2j).
    // This is simple enough: the optimal price is 0.5, with agent j buying j/2 units.
    for (int j = 1; j <= 100; j++) {
        auto c = sim->createAgent<Quadratic>();
        c->coef(m) = 1;
        c->coef(x) = 1;
        c->coef(x, x) = -0.5 / j;
        consumers.push_back(c);

        // Use MUPD for optimization
        eris_id_t z = sim->createIntraOpt<MUPD>(c, m);
        std::cout << "MUPD: " << z << "\n";

        // Give them some income:
        c->assets() += 100*m1;
        z = sim->createInterOpt<FixedIncome>(c, 100*m1);
        std::cout << "FixedIncome: " << z << "\n";
    }

    for (int i = 0; i < 30; i++) {
        std::cout << "Running iteration " << i << "...\n";
        sim->run();

        std::cout << "done. (" << sim->intraopt_count << " intraopt loops)\n";

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
