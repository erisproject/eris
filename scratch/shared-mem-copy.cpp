#include <eris/Eris.hpp>
#include <eris/Random.hpp>
#include <eris/SharedMember.hpp>
#include <eris/agent/AssetAgent.hpp>

using namespace eris;

class Foo : public agent::AssetAgent {};

class Bar : public Foo {};

int main() {
    Eris<Simulation> sim;

    auto foo = sim->create<Foo>();
    auto bar = sim->create<Bar>();
    auto aa = sim->create<agent::AssetAgent>();

    std::vector<SharedMember<Member>> v;
    for (int i = 0; i < 10; i++) {
        v.push_back(sim->create<Bar>());
    }

    for (auto &b : v) {
        std::cerr << "b->id = " << b->id() << "\n";
    }

    std::shuffle(v.begin(), v.end(), Random::rng());
    std::cerr << "shuffled:\n";
    for (auto &b : v) {
        std::cerr << "b->id = " << b->id() << "\n";
    }


    SharedMember<Foo> f2;
    if (f2) { std::cerr << "wtf?\n"; }
    f2 = foo;
    SharedMember<Foo> f3;
    f3 = foo;
    f2 = f3;
    SharedMember<Member> fm{f2};
    SharedMember<Member> fmz;
    SharedMember<agent::AssetAgent> fa{f3};
    SharedMember<Foo> f4;
    f4 = fa;
    f4 = fm;
    f4 = fa;
    fmz = fa;
    fmz = f4;
    fmz = fm;
    SharedMember<Foo> f_null;
    SharedMember<Member> bn{f_null};
    bn = bar;
//    bn = SharedMember<Member>{};
    int throws = 0;
    try { f4 = bar; }
    catch (std::runtime_error &e) { throws++; }
    try { fm = bar; }
    catch (std::runtime_error &e) { throws++; }

    if (throws != 2)
        throw std::runtime_error("Didn't get enough throws!");

}