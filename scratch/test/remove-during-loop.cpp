#include <vector>
#include <iostream>

using namespace std;

int main() {
    vector<double> v;
    v.push_back(100.7323);
    v.push_back(200.7323);
    v.push_back(300.7323);
    v.push_back(400.7323);

    for (auto it = v.begin(); it != v.end(); ) {
        if (*it >= 200 and *it < 400)
            it = v.erase(it);
        else
            ++it;
    }

    cout << "i:";
    for (auto i : v) {
        cout << " " << i;
    }
    cout << "\n";
}
