#include <eris/Random.hpp>
#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>

using namespace eris;

const std::regex number_re("[1-9]\\d*");

int main(int argc, char *argv[]) {

    if (argc != 3 or not std::regex_match(argv[1], number_re) or not std::regex_match(argv[1], number_re)) {
        std::cerr << "Usage: " << argv[0] << " N K -- generates random-${N}x${K}-${SEED}.csv containing Nx(K+1) random standard normal variables (x1, ..., xK, u)\n";
        return 1;
    }

    unsigned long N = std::stoul(argv[1]);
    unsigned long K = std::stoul(argv[2]);

    auto &seed = Random::seed();
    std::fstream csv("random-" + std::to_string(N) + "x" + std::to_string(K) + "-" + std::to_string(seed) + ".csv",
            std::fstream::out | std::fstream::trunc);
    csv << std::setprecision(std::numeric_limits<double>::max_digits10);

    for (unsigned long i = 1; i <= K; i++) csv << "x" << i << ",";
    csv << "u\n";
    for (unsigned long n = 0; n < N; n++) {
        for (unsigned long k = 0; k < K; k++) {
            csv << Random::rstdnorm() << ",";
        }
        // And finally the error term:
        csv << Random::rstdnorm() << "\n";
    }
}
