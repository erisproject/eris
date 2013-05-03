#include "Eris.hpp"
#include "Simulation.hpp"
#include "consumer/Quadratic.hpp"
#include <iostream>
#include <map>
#include <boost/format.hpp>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using boost::format;

int main() {
    Eris<Simulation> sim;

    // Set up a numeraire good
    eris_id_t money = sim->addGood(new Good::Continuous("Money"));
    // Plus another divisible good
    eris_id_t x = sim->addGood(new Good::Continuous("x"));
    // And a discrete good
    eris_id_t w = sim->addGood(new Good::Discrete("w"));


    // We have just a single consumer, with quaslinear quadratic utility in the x good
    Quadratic c1(0.0, {{money, 1}, {x, 10}, {w, 100}});

    c1.setQuadCoef(money, x, 0.1);
    c1.setQuadCoef(money, w, -0.1);
    c1.setQuadCoef(x, x, -1);
    c1.setQuadCoef(w, w, -1);
    c1.setQuadCoef(x, w, 0.3);

    Bundle b;
    for (int m = 0; m <= 10; m++) {
        b.set(money, 10-m);
        b.set(x, m);
        b.set(w, 1);

        cout << "Fred's u(n="<<10-m<<", x="<<m<<", w="<<2<<") = " << c1.utility(b) << "\n";

        bool first = true;
        cout << "Gradient:";
        for (auto g : c1.gradient({money,x,w}, b)) {
            cout << format(" %10g") % g.second;
        }
        cout << "\n\n";

        first = true;
        for (auto col : c1.hessian({money,x,w}, b)) {
            if (first) { first = false; cout << "Hessian: "; }
            else cout << "         ";

            for (auto h : col.second) {
                cout << format(" %10g") % h.second;
            }
            cout << "\n";
        }
        cout << "\n";
    }

}
