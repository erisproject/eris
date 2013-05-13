// Test file for working out the basic reference structure for
// Eris/Simulation/components Here Eris corresponds to FrobWrap, Simulation
// corresponds to Frob, and Joe is a component that has a weak reference to
// it's owning Frob object.

#include <memory>
#include <iostream>


using namespace std;

class Joe;

class Frob : public std::enable_shared_from_this<Frob> {
    public:
        Frob();
        ~Frob();
        void setJoe(Joe &j);
    private:
        shared_ptr<Joe> joe;
};

class FrobWrap {
    public:
        FrobWrap() { // Creates a Frob
            frob = shared_ptr<Frob>(new Frob());
        }
        FrobWrap(Frob *f) { // Uses the passed-in Frob
            frob = shared_ptr<Frob>(f);
        }
        Frob *operator ->() {
            return frob.get();
        }
    private:
        shared_ptr<Frob> frob;
};


class Joe {
    public:
        Joe() {
            cout << "Constructing Joe\n";
        }
        ~Joe() {
            cout << "Joe dying :'-(\n";
        }
        void setFrob(Frob *f) {
            cout << "Setting Joe's Frob\n";
            myfrob = f->shared_from_this();
            cout << "/setFrob\n";
        }
    private:
        weak_ptr<Frob> myfrob;
};

Frob::Frob() {
    cout << "Constructing Frob!\n";
    cout << "/Frob\n";
}
Frob::~Frob() {
    cout << "Destroying Frob!\n";
}
void Frob::setJoe(Joe &j) {
    cout << "setting joe\n";
    joe = make_shared<Joe>(j);
    cout << "made shared\n";
    joe.get()->setFrob(this);
    cout << "/setJoe\n";
}

void f() {
    FrobWrap f(new Frob);
    Joe j;

    f->setJoe(j);
}

int main() {
    cout << "Calling f()\n";
    f();
    cout << "f() finished\n";
}
