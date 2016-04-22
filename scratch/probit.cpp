#include <eris/random/rng.hpp>
#include <eris/random/normal_distribution.hpp>
#include <eris/random/truncated_normal_distribution.hpp>
#include <eris/belief/BayesianLinear.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace eris;
using namespace eris::belief;
using namespace Eigen;

int main(int argc, char *argv[]) {
    const unsigned k = 2;
    BayesianLinear lin(k);

    MatrixXd sample_x;
    VectorXd sample_u;
    VectorXd beta0(k);
    beta0 << 0, 0.5;

    auto &rng = eris::random::rng();
    eris::random::normal_distribution<double> stdnorm;

    unsigned n = 0;
    // If given a CSV file, load the data from it
    if (argc == 2) {
        std::cout << "Loading data from " << argv[1] << "\n";
        std::fstream csv(argv[1], std::fstream::in);
        std::string line, value;
        std::vector<std::vector<double>> values;
        bool did_header = false;
        while (std::getline(csv, line)) {
            if (not did_header) {
                unsigned fields = std::count(line.begin(), line.end(), ',') + 1;
                if (fields < k) {
                    std::cerr << "Data doesn't seem like a valid CSV file: need at least " << k << " fields.  Aborting!\n";
                    return 2;
                }
                values.resize(fields);
                did_header = true;
            }
            else {
                ++n;
                for (auto &v : values) v.resize(n, std::numeric_limits<double>::quiet_NaN());

                std::istringstream sline(line);
                for (unsigned pos = 0; std::getline(sline, value, ','); pos++) {
                    values.at(pos).at(n-1) = std::stod(value);
                }
            }
        }
        if (n < k) {
            std::cerr << "Error: file contains too few observations (n=" << n << ")\n";
            return 3;
        }
        sample_x = MatrixXd(n, k);
        sample_u = VectorXd(n);
        for (unsigned c = 0; c <= k; c++) {
            for (unsigned r = 0; r < n; r++) {
                if (c == 0)
                    sample_x(r,0) = 1.0;
                else if (c < k)
                    sample_x(r,c) = values.at(c-1).at(r);
                else
                    sample_u(r) = values.at(std::max<unsigned>(c, values.size())-1).at(r);
            }
        }
    }
    // Otherwise generate random data:
    else {
        std::cout << "Generating random data (provide a filename to use existing data)\n";
        n = 1000;
        sample_x = MatrixXd(n, k);
        sample_u = VectorXd(n);
        for (unsigned i = 0; i < n; i++) {
            sample_x(i, 0) = 1.0;
            sample_x(i, 1) = stdnorm(rng);
            sample_u(i) = stdnorm(rng);
        }
    }
    VectorXd sample_y_latent(n);
    // Generate data according to model y* = X beta + e, where X is a constant and a N(0,1) draw,
    // e is also a N(0,1).
    for (unsigned i = 0; i < n; i++) {
        sample_x(i, 0) = 1;
        sample_x(i, 1) = stdnorm(rng);
        sample_y_latent[i] = sample_x.row(i) * beta0 + stdnorm(rng);
    }

    //sample_y_latent = sample_x * beta0 + sample_u;
    Matrix<bool, Dynamic, 1> sample_y = sample_y_latent.unaryExpr([](const double &c) -> bool { return c >= 0; });

    const unsigned burnin = 200, draws = 50000, thinning = 5;
    VectorXd beta_last = VectorXd::Zero(k);
    MatrixXd beta_store(MatrixXd::Zero(k, draws));

    VectorXd ystar_sum(VectorXd::Zero(n));
    const double sigma = 1.0; // Fix at 1 (identification condition)

    for (long d = -(long)burnin; d < draws; d++) {
        for (unsigned thin = thinning; thin > 0; thin--) {
            VectorXd ystar(n);
            for (unsigned i = 0; i < n; i++) {
                double lower_bound = (sample_y[i] ? 0 : -std::numeric_limits<double>::infinity()),
                       upper_bound = (sample_y[i] ? std::numeric_limits<double>::infinity() : 0);
                double mean = sample_x.row(i) * beta_last;
                ystar[i] = random::truncated_normal_distribution<double>(mean, sigma, lower_bound, upper_bound)(rng);
            }
            //ystar_sum += ystar;

            //auto post_star = lin.update(ystar, sample_x);
            auto draw = lin.update(ystar, sample_x).draw();
            beta_last = draw.head(k);
            if (d >= 0 and thin == 1) {
                beta_store.col(d) = beta_last;
            }
        }
    }

    VectorXd beta_mean = beta_store.rowwise().mean();
    VectorXd beta_stdev;
    {
        VectorXd demean_sum_sq  = (beta_store.colwise() - beta_mean).rowwise().sum().cwiseAbs2();
        VectorXd demean_sq_sum = (beta_store.colwise() - beta_mean).cwiseAbs2().rowwise().sum();
        beta_stdev = ((demean_sq_sum - demean_sum_sq / beta_store.cols()) / (beta_store.cols()-1)).cwiseSqrt();
    }

    std::cout << "posterior mean:    " << beta_mean.transpose() << "\n";
    std::cout << "posterior st.dev.: " << beta_stdev.transpose() << "\n";
}
