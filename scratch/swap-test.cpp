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
    auto sim = Simulation::spawn;
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
        // gm is actually an A now, so this should be a bad cast:
        SharedMember<G> g3(gm);
        std::cout << "Uh oh, why didn't that throw?\n";
    }
    catch (std::bad_cast &e) {
        std::cout << "Great, caught the right exception: " << e.what() << "\n";
    }

    SharedMember<Member> a3(a);
    SharedMember<A>(a3)->foo();
    SharedMember<A>(gm)->foo();
    try {
        SharedMember<A>(am)->foo();
        std::cout << "Uh oh, why didn't cast to A throw?\n";
    }
    catch (std::bad_cast &e) {
        std::cout << "Good, caught exception: " << e.what() << "\n";
    }
    a3 = am; // Should be allowed
    SharedMember<G>(a3)->bar();
    SharedMember<G>(am)->bar();
    try {
        SharedMember<G>(gm)->bar();
        std::cout << "Uh oh, why didn't cast to G throw?\n";
    }
    catch (std::bad_cast &e) {
        std::cout << "Good, caught exception: " << e.what() << "\n";
    }

    std::cout << "Tada!\n";
}
