#include <eris/random/rng.hpp>
#include <random>
#include <iostream>

int main() {
    std::normal_distribution<double> norm;

    for (int i = 0; i < 5; i++) {
        std::cout << norm(eris::random::rng()) << "\n";
    }
}
