#include <eris/random/rng.hpp>
#include <random>
#include <iostream>
#include <functional>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>
#include <chrono>

using clk = std::chrono::high_resolution_clock;
using dur = std::chrono::duration<double>;

int main(int argc, char *argv[]) {
    auto &rng = eris::random::rng();

    std::string th_mean, th_var, th_skew, th_kurt;
    std::function<double()> gen;
    if (argc > 1) {
        std::string which(argv[1]);
        if (which == "N" or which == "n") {
            gen = [&rng]() { return boost::random::normal_distribution<double>()(rng); };
            th_mean = "0"; th_var = "1"; th_skew = "0"; th_kurt = "3";
            std::cout << "Drawing from N(0,1)\n";
        }
        else if (which == "U" or which == "u") {
            gen = [&rng]() { return boost::random::uniform_real_distribution<double>()(rng); };
            th_mean = "0.5"; th_var = "0.08333..."; th_skew = "0"; th_kurt = "1.8";
            std::cout << "Drawing from U[0,1)\n";
        }
        else if (which == "E" or which == "e") {
            gen = [&rng]() { return boost::random::exponential_distribution<double>()(rng); };
            th_mean = "1"; th_var = "1"; th_skew = "2"; th_kurt = "9";
            std::cout << "Drawing from Exp(1)\n";
        }
        else {
            std::cerr << "Unknown distribution `" << which << "'\n";
        }
    }
    if (not gen) {
        std::cerr << "Usage: " << argv[0] << " {E,N,U} -- generate and summarize 5 million draws from a distribution\n";
        std::cerr << "Usage: " << argv[0] << " {E,N,U} -n -- report mean of 200 million draws from a distribution\n";
        std::cerr << "\nDistributions: E - Exponential(1); N - Normal(0,1); U - Uniform[0,1)\n";

        exit(1);
    }

    std::cout << "seed: " << eris::random::seed() << "\n";
    std::cout.precision(std::numeric_limits<double>::max_digits10);

    bool sum_only = false;
    if (argc > 2 and std::string(argv[2]) == "-n") {
        sum_only = true;
    }

    constexpr int count = 5000000;

    if (sum_only) {
        double mean = 0;
        // If we don't have to calculate all that other crap and store all the values, generation is
        // much faster (somewhere around 20 times).
        constexpr int sumcount = 40*count;
        auto start = clk::now();
        for (int i = 0; i < sumcount; i++) {
            mean += gen();
        }
        auto end = clk::now();
        double seconds = dur(end - start).count();
        mean /= sumcount;
        std::cout << "mean: " << mean << "\n";
        std::cout << "Elapsed: " << seconds << "\n";
        exit(0);
    }

    std::vector<double> draws(count);

    for (int i = 0; i < count; i++) {
        draws[i] = gen();
        std::cout << draws[i] << "\n";
    }

    double mean = std::accumulate(draws.begin(), draws.end(), 0.0) / count;
    double e2 = 0, e3 = 0, e4 = 0;
    for (int i = 0; i < count; i++) {
        double demean = draws[i] - mean;
        double demean2 = demean*demean;
        e2 += demean2;
        e3 += demean2*demean;
        e4 += demean2*demean2;
    }
    e2 /= count;
    e3 /= count;
    e4 /= count;

    double var = e2;
    double skew = e3 / std::pow(e2, 1.5);
    double kurt = e4 / (e2*e2);
    std::cout << "=======\n" <<
        "mean: " << mean << " (theory: " << th_mean << ")\n" <<
        "variance: " << var << " (theory: " << th_var << ")\n" <<
        "skewness: " << skew << " (theory: " << th_skew << ")\n" <<
        "kurtosis: " << kurt << " (theory: " << th_kurt << ")\n";
}
