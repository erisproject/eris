#include <utility>
#include <iostream>
#include <vector>
#include <list>
#include <typeinfo>

class Bar {};
class Mid : public Bar {};
class Foo {
    public:
        Foo(int a) : a(a) {}
        int a;
        typedef Mid member_type;
};

template <class Container,
          class SharedMemberContainer = typename std::enable_if<
            std::is_base_of<Bar, typename decltype(std::declval<Container>().begin())::value_type::member_type>::value
          >::type>
int sum(const Container &c) {
    int sum = 0;
    std::vector<Foo> v(c.begin(), c.end());
    for (auto &c: v) {
        sum += c.a;
    }

    return sum;
}



int main() {
    std::vector<Foo> a;
    std::vector<Foo> b;
    std::list<Foo> c;

    std::cout << typeid(Foo).name() << "\n";
    std::cout << typeid(decltype(std::declval<std::vector<Foo>>().begin())::value_type).name() << "\n";
    std::cout << std::is_same<decltype(std::declval<std::vector<Foo>>().begin())::value_type, Foo>::value << "\n";

    a.push_back(Foo(1));
    a.push_back(Foo(10));
    a.push_back(Foo(100));

    b.push_back(Foo(1000));
    b.push_back(Foo(10000));

    c.push_back(Foo(100000));

    std::cout << sum(a) << "\n";
    std::cout << sum(b) << "\n";
    std::cout << sum(c) << "\n";
//    sum(Foo(4));
}
