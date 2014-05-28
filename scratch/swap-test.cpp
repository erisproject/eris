#include <eris/Eris.hpp>
#include <eris/SharedMember.hpp>
#include <eris/Agent.hpp>
#include <eris/Good.hpp>
#include <iostream>
#include <utility>

using namespace eris;
using std::swap;

class A : public Agent {
    public:
        void foo() { std::cout << "This is agent!\n"; }
};
class G : public Good::Continuous {
    public:
        void bar() { std::cout << "This is good!\n"; }
};

int main() {
    Eris<Simulation> sim;
    auto a = sim->create<A>();
    auto g = sim->create<G>();

    auto am = SharedMember<Member>(a);
    auto gm = SharedMember<Member>(g);

    swap(am, gm);

    SharedMember<A> a2(gm);
    SharedMember<G> g2(am);

    a2->foo();
    g2->bar();
    a->foo();
    g->bar();

    try {
        SharedMember<G> g3(gm);
        std::cout << "Uh oh, why didn't that throw?\n";
    }
    catch (std::bad_cast &e) {
        std::cout << "Great, caught the right exception: " << e.what() << "\n";
    }

    try {
        SharedMember<Member> a3(a);
        a3 = am;
        std::cout << "Uh oh, why didn't a3 = am throw?\n";
    }
    catch (std::runtime_error &e) {
        std::cout << "Good, caught exception: " << e.what() << "\n";
    }

    std::cout << "Tada!\n";
}
