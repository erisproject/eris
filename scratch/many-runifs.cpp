#include <eris/random/rng.hpp>
#include <random>
#include <iostream>
#include <boost/random/uniform_real_distribution.hpp>

int main() {
    boost::random::uniform_real_distribution<double> runif(0, 2);

    auto &rng = eris::random::rng();
    double sum = 0;
    for (int i = 0; i < 50000000; i++) {
        sum += runif(rng);
    }
    std::cout.precision(15);
    std::cout << "sum of 50,000,000 U[0,2] = " << sum << "\n";
}
