#include <eris/Random.hpp>
#include <eris/belief/BayesianLinear.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/complement.hpp>
#include <iostream>

using namespace eris;
using namespace eris::belief;
using namespace Eigen;

int main() {
    const unsigned k = 2;
    BayesianLinear lin(k);

    MatrixXd foo = MatrixXd::Random(4, 20);
    VectorXd bar = VectorXd::Ones(4);
    MatrixXd foobar = (foo.colwise() + bar).rowwise().sum().cwiseAbs2();
    std::cout << "foo =\n" << foo << "\nbar =\n" << bar << "\nfoobar =\n" << foobar << "\n";

    // Sample data:
    const unsigned n = 1000;
    VectorXd beta0(k);
    beta0 << 0, 0.5;
    VectorXd sample_y_latent(n);
    MatrixXd sample_x(n, k);

    // Generate data according to model y* = X beta + e, where X is a constant and a N(0,1) draw,
    // e is also a N(0,1).
    for (unsigned i = 0; i < n; i++) {
        sample_x(i, 0) = 1;
        sample_x(i, 1) = Random::rstdnorm();
        sample_y_latent[i] = sample_x.row(i) * beta0 + Random::rstdnorm();
    }

    Matrix<bool, Dynamic, 1> sample_y = sample_y_latent.unaryExpr([](const double &c) -> bool { return c >= 0; });

    const unsigned burnin = 1000, draws = 10000;
    VectorXd beta_last = VectorXd::Zero(k);
    MatrixXd beta_store(MatrixXd::Zero(k, draws));

    VectorXd ystar_sum(VectorXd::Zero(n));
    double s2 = 1.0; // Leave fixed at 0 (identification condition)

    for (long d = -(long)burnin; d < draws; d++) {
        VectorXd ystar(n);
        for (unsigned i = 0; i < n; i++) {
            double lower_bound = (sample_y[i] ? 0 : -std::numeric_limits<double>::infinity()),
                   upper_bound = (sample_y[i] ? std::numeric_limits<double>::infinity() : 0);
            double mean = sample_x.row(i) * beta_last;
            boost::math::normal_distribution<double> norm_dist(mean, s2);
            std::normal_distribution<double> rnorm(mean, s2);
            ystar[i] = Random::truncDist(norm_dist, rnorm, lower_bound, upper_bound, mean);
        }
        ystar_sum += ystar;

        auto post_star = lin.update(ystar, sample_x);
        auto &draw = post_star.draw();
        beta_last = draw.head(k);
        std::cout << "d=" << d << "\n";
        if (d >= 0) {
            beta_store.col(d) = beta_last;
        }
        if (true or d % 10 == 0) {
            std::cout << "posterior parameters: beta = " << post_star.beta().transpose() << ", s2 = " << post_star.s2() << "\n";
            std::cout << "draw: beta{" << d << "} = " << beta_last.transpose() << ", s2 = " << s2 << "\n";
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
