#include <eris/algorithms.hpp>
#include <map>
#include <iostream>

int main() {
    std::multimap<int, int> foo;

    foo.insert(std::make_pair(3, 4));
    foo.insert(std::make_pair(3, 5));
    foo.insert(std::make_pair(3, 6));
    foo.insert(std::make_pair(4, 7));
    foo.insert(std::make_pair(4, 8));
    foo.insert(std::make_pair(5, 9));

    for (auto f: eris::range(foo.equal_range(3))) {
        std::cout << "foo " << f.first << " value: " << f.second << "\n";
    }
}
