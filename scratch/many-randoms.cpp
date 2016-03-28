#include <eris/random/rng.hpp>
#include <random>
#include <iostream>
#include <functional>
#include <boost/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/random/exponential_distribution.hpp>

int main(int argc, char *argv[]) {
    auto &rng = eris::random::rng();
    std::cout << "seed: " << eris::random::seed() << "\n";

    std::string th_mean, th_var, th_skew, th_kurt;
    std::function<double()> gen;
    if (argc > 1) {
        std::string which(argv[1]);
        if (which == "N" or which == "n") {
            gen = [&rng]() { return boost::random::normal_distribution<double>()(rng); };
            th_mean = "0"; th_var = "1"; th_skew = "0"; th_kurt = "3";
        }
        else if (which == "U" or which == "u") {
            gen = [&rng]() { return boost::random::uniform_real_distribution<double>()(rng); };
            th_mean = "0.5"; th_var = "0.08333..."; th_skew = "0"; th_kurt = "1.8";
        }
        else if (which == "E" or which == "e") {
            gen = [&rng]() { return boost::random::exponential_distribution<double>()(rng); };
            th_mean = "1"; th_var = "1"; th_skew = "2"; th_kurt = "9";
        }
        else {
            std::cerr << "Unknown distribution `" << which << "'\n";
        }
    }
    if (not gen) {
        std::cerr << "Usage: " << argv[0] << " {E,N,U} -- generate and summarize 1 million draws from exponential, normal, or uniform distribution\n";
        exit(1);
    }

    constexpr int count = 1000000;
    std::vector<double> draws(count);

    std::cout.precision(std::numeric_limits<double>::max_digits10);
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
