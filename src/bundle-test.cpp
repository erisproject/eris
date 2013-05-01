#include "Eris.hpp"
#include "Simulation.hpp"
#include "Bundle.hpp"
#include <string>
#include <iostream>

using namespace std;

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

    eris_id_t g1 = sim->addGood(new Good::Continuous());
    eris_id_t g2 = sim->addGood(new Good::Continuous());
    eris_id_t g3 = sim->addGood(new Good::Continuous());
    eris_id_t g4 = sim->addGood(new Good::Continuous());

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
}
