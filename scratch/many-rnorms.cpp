#include <eris/Random.hpp>
#include <random>
#include <iostream>

int main() {
    std::normal_distribution<double> norm;

    auto &rng = eris::Random::rng();
    double sum = 0;
    for (int i = 0; i < 50000000; i++) {
        sum += norm(rng);
    }
    std::cout << "sum of 50,000,000 N(0,1) = " << sum << "\n";
}
