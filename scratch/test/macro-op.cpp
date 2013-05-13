#define foo(OP) \
double f() { return 1 OP 2; }

foo(-)

#include <iostream>

using namespace std;

int main() {
    cout << f() << "\n";
}
