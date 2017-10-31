// Test script to work out how to get all combinations of size 2 or above from a set of elements.
//
#include <eris/algorithms.hpp>
#include <eris/types.hpp>
#include <set>
#include <unordered_set>
#include <stack>
#include <vector>
#include <iostream>

using std::cout;

void print_vec(const std::vector<eris::id_t> &v) {
    cout << "[";
    bool first = true;
    for (auto vi : v) {
        if (first) first = false;
        else cout << ",";
        cout << vi;
    }
    cout << "]\n";
}


int main() {

    std::set<eris::id_t> permute;
    permute.insert(33);
    permute.insert(44);
    permute.insert(55);
    permute.insert(66);
    permute.insert(77);
    permute.insert(88);
    permute.insert(99);

    eris::all_combinations(permute.begin(), permute.end(), print_vec);
    eris::all_combinations(permute.crbegin(), permute.crend(),
            [](const std::vector<eris::id_t> &comb) -> void { print_vec(comb); });

    std::unordered_set<eris::id_t> pb;
    pb.insert(33);
    pb.insert(44);
    eris::all_combinations(pb.cbegin(), pb.cend(), print_vec);
}
