#include "Eris.hpp"
#include "Simulation.hpp"
#include "Bundle.hpp"
#include <string>
#include <iostream>

using namespace std;
using namespace eris;

void _printBundle(string name, const BundleNegative &b) {
    cout << "Bundle " << name << " contents:\n";
    for (auto g : b) {
        cout << "    " << g.first << ": " << g.second << "\n";
    }
}
#define printBundle(b) _printBundle(#b, b)

#define print(b) \
    cout << #b " = " << (b) << "\n";

int main() {
    Eris<Simulation> sim;

    auto g1 = sim->addGood(Good::Continuous());
    auto g2 = sim->addGood(Good::Continuous());
    auto g3 = sim->addGood(Good::Continuous());
    auto g4 = sim->addGood(Good::Continuous());

    Bundle b[10];

    b[0].set(g1, 1);
    b[0] *= 3;
    b[0].set(g2, 12);

    b[1].set(g1, 4);
    b[1].set(g3, 1);

    b[2].set(g1, 6);
    b[2].set(g2, 6);
    b[2].set(g3, 0.1);

    b[3].set(g1, 1);
    b[3].set(g2, 1);

    b[4] = b[0] + b[1];

    printBundle(b[0]);
    printBundle(b[0] * 2);
    printBundle(2 * b[0]);
    printBundle(b[0] + b[1]);
    printBundle(b[0] + b[1] - b[2]);

    print(b[0] > b[0]);
    print(b[0] >= b[0]);
    print(b[0] == b[0]);
    print(b[0] <= b[0]);
    print(b[0] < b[0]);
    print(b[0] != b[0]);
    
    print(b[0] > b[1]);
    print(b[0] >= b[1]);
    print(b[0] == b[1]);
    print(b[0] <= b[1]);
    print(b[0] < b[1]);
    print(b[0] != b[1]);

    print(b[4] > b[1]);
    print(b[4] >= b[1]);
    print(b[4] == b[1]);
    print(b[4] <= b[1]);
    print(b[4] < b[1]);
    print(b[4] != b[1]);
    print(b[1] > b[4]);
    print(b[1] >= b[4]);
    print(b[1] == b[4]);
    print(b[1] <= b[4]);
    print(b[1] < b[4]);
    print(b[1] != b[4]);

    print(b[4] > b[2]);
    print(b[4] >= b[2]);
    print(b[4] == b[2]);
    print(b[4] <= b[2]);
    print(b[4] < b[2]);
    print(b[4] != b[2]);
    print(b[2] > b[4]);
    print(b[2] >= b[4]);
    print(b[2] == b[4]);
    print(b[2] <= b[4]);
    print(b[2] < b[4]);
    print(b[2] != b[4]);
//    print(
//    cout << "b > c: " << (b > c) << "\nd > e: " << (d > e) << "\nb + c > d: " << (b + c > d) << "\n";

    BundleNegative f = -(b[0] + b[1]);

    printBundle(f);

    printBundle(b[4] * 2);
    printBundle(b[4] / 3);

    bool good1 = false, good2 = false;
    try { b[3].set(g3, -3); }
    catch (Bundle::negativity_error e) { good1 = true; }
    try { b[3].set({{*g4,1}, {*g3,-1}}); }
    catch (Bundle::negativity_error e) { good2 = true; }

    cout << "Caught negativity errors:\n";
    print(good1);
    print(good2);
    printBundle(b[3]);


    b[5].set({{ *g1, 2 }, {*g2, 3}});
    b[6].set({{ *g1, 1 }, {*g2, 4}, {*g3, 7}});
    b[7] = Bundle({{ *g1, 1 }, {*g2, 1}, {*g3, 1}, {*g4, 1}});

    printBundle(b[5]);
    printBundle(b[6]);
    printBundle(b[7]);
    print(b[5] / b[6]);
    try {printBundle(b[5] % b[6]);} catch (Bundle::negativity_error e) {}
    print(b[6] / b[5]);
    try {printBundle(b[6] % b[5]);} catch (Bundle::negativity_error e) {}
    print(b[5] / b[7]);
    try {printBundle(b[5] % b[7]);} catch (Bundle::negativity_error e) {}
    print(b[7] / b[5]);
    try {printBundle(b[7] % b[5]);} catch (Bundle::negativity_error e) {}
    print(b[6] / b[7]);
    try {printBundle(b[6] % b[7]);} catch (Bundle::negativity_error e) {}
    print(b[7] / b[6]);
    try {printBundle(b[7] % b[6]);} catch (Bundle::negativity_error e) {}
}
