#include <eris/Simulation.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/firm/PriceFirm.hpp>
#include <eris/Good.hpp>
#include <iostream>
#include <map>
#include <boost/format.hpp>

using namespace std;
using namespace eris;
using namespace eris::consumer;
using namespace eris::firm;
using boost::format;

int main() {
    auto sim = Simulation::create();

    // Set up a numeraire good
    auto money = sim->spawn<Good::Continuous>("Money");
    // Plus another divisible good
    auto x = sim->spawn<Good::Continuous>("x");
    // And a discrete good
    auto w = sim->spawn<Good::Discrete>("w");


    std::map<eris::id_t, double> init;
    init[money] = 1;
    init[x] = 10;
    init[w] = 100;
    // We have just a single consumer, with quaslinear quadratic utility in the x good
    auto c1 = sim->spawn<Quadratic>(init, 0.0);

    c1->coef(money, x) = -1.1;
    c1->coef(money, w) = -0.1;
    c1->coef(x, x) = -1;
    c1->coef(w, w) = -1;
    c1->coef(x, w) = 0.3;

    // And a price-setting firm:
    PriceFirm f1(Bundle(x, 1), Bundle(money, 2));


}
