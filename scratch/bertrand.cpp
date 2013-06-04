#include <eris/Eris.hpp>
#include <eris/Simulation.hpp>
#include <eris/Agent.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/market/Bertrand.hpp>
#include <eris/firm/PriceFirm.hpp>
#include <iostream>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using namespace eris::firm;
using namespace eris::market;

int main() {
    Eris<Simulation> sim;
    for (int i = 0; i < 10; i++) {
        sim->createAgent<Quadratic>(1.0);
    }

    auto money = sim->createGood<Good::Continuous>("money");
    auto cgood = sim->createGood<Good::Continuous>("continuous good");
    auto dgood = sim->createGood<Good::Discrete>("discrete good");

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


    auto bertrand = sim->createMarket<Bertrand>(Bundle(cgood, 1), Bundle(money, 1), true);

    auto f1 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 1), 0.4);
    auto f2 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 1), 0.2);
    auto f3 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 10), 0.2);
    auto f4 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 10), 0.1);
    auto f5 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 100));
    auto f6 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 100), 0.01);
    auto f7 = sim->createAgent<PriceFirm>(Bundle(cgood, 1), Bundle(money, 100), 0.04);
    bertrand->addFirm(f1);
    bertrand->addFirm(f2);
    bertrand->addFirm(f3);
    bertrand->addFirm(f4);
    bertrand->addFirm(f5);
    bertrand->addFirm(f6);
    bertrand->addFirm(f7);

    cout << "Betrand price for q=1 is: " << bertrand->price(1).total << " (should be 13.6)\n";
}
