// Test script to work out how to get all combinations of size 2 or above from a set of elements.
//
#include <eris/algorithms.hpp>
#include <set>
#include <unordered_set>
#include <stack>
#include <vector>
#include <iostream>

using std::cout;

void print_vec(const std::vector<int> &v) {
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

    std::set<int> permute;
    permute.insert(33);
    permute.insert(44);
    permute.insert(55);
    permute.insert(66);
    permute.insert(77);
    permute.insert(88);
    permute.insert(99);

    eris::all_combinations(permute.begin(), permute.end(), print_vec);
    eris::all_combinations(permute.crbegin(), permute.crend(),
            [](const std::vector<int> &comb) -> void { print_vec(comb); });

    std::unordered_set<int> pb;
    pb.insert(33);
    pb.insert(44);
    eris::all_combinations(pb.cbegin(), pb.cend(), print_vec);
/*
    std::vector<int> combination;
    std::stack<std::set<int>::const_iterator> it_stack;
    auto end = permute.cend();

    it_stack.push(permute.cbegin());

    int iters = 0;
    while (!it_stack.empty()) {
        iters++;
        auto it = it_stack.top();
        combination.push_back(*it);
        if (combination.size() > 1) {
            // DO SOMETHING HERE
            print_vec(combination);
        }
        auto n = std::next(it);
        if (n == end) {
            // We're at the end, so we need to pop ourselves and our value off the stack
            combination.pop_back();
            it_stack.pop();
            // Also pop off the previous value and increment it (the incremented value is going to
            // be pushed back on in the next iteration).
            combination.pop_back();
            if (!it_stack.empty()) it_stack.top()++;
        }
        else
            it_stack.push(n);
    }

    cout << iters << " loop iterations\n";
    */
}
