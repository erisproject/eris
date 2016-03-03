#include <eris/random/rng.hpp>
#include <random>
#include <iostream>
#include <boost/random/normal_distribution.hpp>

int main() {
    boost::random::normal_distribution<double> norm;

    auto &rng = eris::random::rng();
    double sum = 0;
    for (int i = 0; i < 50000000; i++) {
        sum += norm(rng);
    }
    std::cout.precision(15);
    std::cout << "sum of 50,000,000 N(0,1) = " << sum << "\n";
}
