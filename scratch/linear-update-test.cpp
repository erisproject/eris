#include <eris/belief/BayesianLinear.hpp>
#include <eris/Random.hpp>
#include <Eigen/QR>
#include <Eigen/SVD>
#include <iostream>
#include <iomanip>

#define print_model(M) \
    std::cout << #M ":\n" << \
        u8"    β_: " << M.beta().transpose() << "\n" << \
        u8"    n_: " << M.n() << "\n" << \
        u8"    s²_: " << M.s2() << "\n" << \
        u8"    V⁻¹: " << MatrixXd(M.Vinv()).format(IOFormat(StreamPrecision, 0, " ", "\n         ")) << "\n"

#define print_OLS(name, beta, X, y) std::cout << name << ":\n" << \
    u8"    β^: " << beta.transpose() << "\n" << \
    u8"    σ^²: " << (y - X * beta).squaredNorm() / X.rows() << "\n" << \
    u8"    X'X: " << (X.transpose() * X).format(IOFormat(StreamPrecision, 0, " ", "\n         ")) << "\n"

    
using namespace eris::belief;
using namespace Eigen;
using namespace eris;

int main() {

    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << std::fixed;
    BayesianLinear foo(3);

    VectorXd beta(3);
    beta << -1, 4, 0.5;
    MatrixXd X(100, 3);
    VectorXd y(100);
    VectorXd u(100);

    auto &rng = Random::rng();
    std::normal_distribution<double> stdnormal;

    for (int t = 0; t < 100; t++) {
        for (int k = 0; k < 3; k++) {
            X(t,k) = stdnormal(rng);
        }
        u[t] = 2.5*stdnormal(rng);
    }

    y = X * beta + u;

    VectorXd betahat = X.jacobiSvd(ComputeThinU | ComputeThinV).solve(y);
 
    print_OLS("OLS", betahat, X, y);


    BayesianLinear foo_100_oneshot(foo, y, X);

    BayesianLinear foo_100_twoshot(foo, y.head(50), X.topRows(50));
    foo_100_twoshot = BayesianLinear(std::move(foo_100_twoshot), y.tail(50), X.bottomRows(50));

    BayesianLinear foo_100_fiveshot = foo;
    for (int i = 0; i < 100; i += 20) {
        foo_100_fiveshot = BayesianLinear(std::move(foo_100_fiveshot), y.segment(i, 20), X.middleRows(i, 20));
    }

    BayesianLinear foo_100_tenshot = foo;
    for (int i = 0; i < 100; i += 10) {
        foo_100_tenshot = BayesianLinear(std::move(foo_100_tenshot), y.segment(i, 10), X.middleRows(i, 10));
    }

    BayesianLinear foo_100_hundredshot = foo;
    for (int i = 0; i < 100; i++) {
        foo_100_hundredshot = BayesianLinear(std::move(foo_100_hundredshot), y.row(i), X.row(i));
    }

    print_model(foo_100_oneshot);
    print_model(foo_100_twoshot);
    //print_model(foo_100_fiveshot);
    print_model(foo_100_tenshot);
    print_model(foo_100_hundredshot);

    BayesianLinear foo_100_weakened_fiftyshot(foo, y.head(50), X.topRows(50));
    foo_100_weakened_fiftyshot = BayesianLinear(std::move(foo_100_weakened_fiftyshot), y.tail(50), X.bottomRows(50), 2.0);

    MatrixXd Xw = X;
    Xw.topRows(50) *= 0.5;
    VectorXd yw = y;
    yw.head(50) *= 0.5;
    BayesianLinear foo_100_weakened_direct(foo, yw, Xw);

    print_model(foo_100_weakened_direct);
    print_model(foo_100_weakened_fiftyshot);

    VectorXd betahat_weakhalf = Xw.jacobiSvd(ComputeThinU | ComputeThinV).solve(yw);
    std::cout << "OLS with pre-scaled prior data:\n";
    print_OLS("OLS (scale-weakened first half)", betahat_weakhalf, Xw, yw);
}
