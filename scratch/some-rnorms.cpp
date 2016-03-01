#include <eris/random/rng.hpp>
#include <random>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <thread>
#include <list>

std::mutex m;

void somernorms(int count) {
    std::normal_distribution<double> norm;
    auto &rng = eris::random::rng();
    for (int i = 0; i < count; i++) {
        double n = norm(rng);
        std::lock_guard<std::mutex> lock(m);
        std::cout << n << std::endl;
    }
}

int main() {

    std::cout << std::setprecision(25);

    std::list<std::thread> th;
    th.emplace_back([]() { somernorms(5); });
    th.emplace_back([]() { somernorms(5); });
    th.emplace_back([]() { somernorms(5); });
    th.emplace_back([]() { somernorms(5); });
    for (auto &t : th) t.join();
}
