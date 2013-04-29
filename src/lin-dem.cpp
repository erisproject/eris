#include "Eris.hpp"
#include "Simulation.hpp"
#include "agent/consumer/Polynomial.hpp"
#include <iostream>
#include <map>
#include <boost/format.hpp>

using namespace std;
using boost::format;

int main() {
    Eris<Simulation> sim;
    sim->addAgent(new Agent());

    // Set up a numeraire good
    eris_id_t money = sim->addGood(new Good::Continuous("Money"));
    // Plus another divisible good
    eris_id_t x = sim->addGood(new Good::Continuous("x"));
    // And a discrete good
    eris_id_t w = sim->addGood(new Good::Discrete("w"));


    // We have just a single consumer, with quaslinear quadratic utility in the x good
    // FIXME: Polynomial should be polynomial, and there should be a matrix of coefficients (to allow cross-product terms).
    Polynomial fred;
    fred[money] = {1};
    fred[x] = {5, -1};
    fred[w] = {2, -1};

    Bundle b;
    for (int m = 0; m <= 10; m++) {
        b.set(money, 10-m);
        b.set(x, m);
        b.set(w, 1);

        cout << "Fred's u(n="<<10-m<<", x="<<m<<", w="<<2<<") = " << fred.utility(b) << "\n";

        bool first = true;
        cout << "Gradient:";
        for (auto g : fred.gradient({money,x,w}, b)) {
            cout << format(" %10g") % g.second;
        }
        cout << "\n\n";

        first = true;
        for (auto col : fred.hessian({money,x,w}, b)) {
            if (first) { first = false; cout << "Hessian: "; }
            else cout << "         ";

            for (auto h : col.second) {
                cout << format(" %10g") % h.second;
            }
            cout << "\n";
        }
        cout << "\n";
    }

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
