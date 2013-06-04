#include <eris/Eris.hpp>
#include <eris/Simulation.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <iostream>
#include <map>
#include <boost/format.hpp>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using boost::format;

void printGoodId(const Good &g) {
    cout << "good.id() = " << g.id() << "\n";
    cout << "(eris_id_t) good = " << (eris_id_t) g << "\n";
    cout << "good.name = " << g.name << "\n";
}

int main() {
    Eris<Simulation> sim;

    // Set up a numeraire good
    auto money = sim->createGood<Good::Continuous>("Money");
    // Plus another divisible good
    auto x = sim->createGood<Good::Continuous>("x");
    // And a discrete good
    auto w = sim->createGood<Good::Discrete>("w");


    // We have just a single consumer, with quaslinear quadratic utility in the x good
    auto c1 = sim->createAgent<Quadratic>();
    c1->coef(money) = 1;
    c1->coef(x) = 10;
    c1->coef(w) = 100;
    c1->coef(money, x) = 0.1;
    c1->coef(money, w) = -0.1;
    c1->coef(x, x) = -1;
    c1->coef(w, w) = -1;
    c1->coef(x, w) = 0.3;

    printGoodId(money);
    printGoodId(x);
    printGoodId(w);

    Bundle b;
    for (int m = 0; m <= 10; m++) {
        b.set(money, 10-m);
        b.set(x, m);
        b.set(w, 1);

        cout << "Fred's u(n="<<10-m<<", x="<<m<<", w="<<2<<") = " << c1->utility(b) << "\n";

        bool first = true;
        cout << "Gradient:";
        for (auto g : c1->gradient({money,x,w}, b)) {
            cout << format(" %10g") % g.second;
        }
        cout << "\n\n";

        first = true;
        for (auto col : c1->hessian({money,x,w}, b)) {
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
