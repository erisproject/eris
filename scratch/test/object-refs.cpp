#include <string>
#include <iostream>

using namespace std;

class Foo {
    public:
        Foo(string a, int b) : a(a), b(b) {}
        string getA() const { return a; }
        int getB() const { return b; }
        void incrB() { ++b; }
    private:
        string a;
        int b;
};

void asdf(Foo const &f) {
    f.incrB();

    cout << "asdf just incremented: " << f.getB() << "\n";
}

int main() {
    Foo f("foo", 12);

    f.incrB();

    cout << "one increment: " << f.getB() << "\n";

    asdf(f);

    cout << "+ remote increment: " << f.getB() << "\n";
}
