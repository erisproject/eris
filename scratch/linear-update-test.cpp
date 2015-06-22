#include <eris/belief/BayesianLinear.hpp>
#include <eris/matrix/EigenImpl.hpp>
#include <eris/Random.hpp>
#include <iostream>
#include <iomanip>

#define print_model(M) \
    std::cout << #M ":\n" << \
        u8"    β_: " << M.beta().transpose() << "\n" << \
        u8"    n_: " << M.n() << "\n" << \
        u8"    s²_: " << M.s2() << "\n" << \
        u8"    V⁻¹: " << M.Vinv().str(std::numeric_limits<double>::max_digits10, ", ", "\n         ") << "\n"

#define print_OLS(name, beta, X, y) std::cout << name << ":\n" << \
    u8"    β^: " << beta.transpose() << "\n" << \
    u8"    σ^²: " << (y - X * beta).squaredNorm() / X.rows() << "\n" << \
    u8"    X'X: " << (X.transpose() * X).str(std::numeric_limits<double>::max_digits10, ", ", "\n         ") << "\n"

    
using namespace eris::belief;
using namespace eris;

int main() {

    Matrix factory = Matrix::create<eris::matrix::EigenImpl>(0, 0);
    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10) << std::fixed;
    BayesianLinear foo(3, factory);

    Vector beta = factory.createVector(3);
    beta[0] = -1; beta[1] = 4; beta[2] = 0.5;
    Matrix X = factory.create(100, 3);
    Vector y = factory.createVector(100);
    Vector u(y);

    auto &rng = Random::rng();
    std::normal_distribution<double> stdnormal;

    for (int t = 0; t < 100; t++) {
        for (int k = 0; k < 3; k++) {
            X(t,k) = stdnormal(rng);
        }
        u[t] = 2.5*stdnormal(rng);
    }

    y = X * beta + u;

    Vector betahat_solve = (X.transpose() * X).solve(X.transpose() * y);
    Vector betahat_solveLS = X.solveLeastSquares(y);

    print_OLS("OLS (solve)", betahat_solve, X, y);
    print_OLS("OLS (solveLS)", betahat_solveLS, X, y);


    BayesianLinear foo_100_oneshot = foo.update(y, X);

    BayesianLinear foo_100_twoshot = foo.update(y.head(50), X.top(50));
    foo_100_twoshot = foo_100_twoshot.update(y.tail(50), X.bottom(50));

    BayesianLinear foo_100_fiveshot = foo;
    for (int i = 0; i < 100; i += 20) {
        foo_100_fiveshot = foo_100_fiveshot.update(y.head(20, i), X.top(20, i));
    }

    BayesianLinear foo_100_tenshot = foo;
    for (int i = 0; i < 100; i += 10) {
        foo_100_tenshot = foo_100_tenshot.update(y.head(10, i), X.top(10, i));
    }

    BayesianLinear foo_100_hundredshot = foo;
    for (int i = 0; i < 100; i++) {
        foo_100_hundredshot = foo_100_hundredshot.update(y.row(i), X.row(i));
    }

    print_model(foo_100_oneshot);
    print_model(foo_100_twoshot);
    //print_model(foo_100_fiveshot);
    print_model(foo_100_tenshot);
    print_model(foo_100_hundredshot);

    BayesianLinear foo_100_weakened_fiftyshot = foo.update(y.head(50), X.top(50));
    foo_100_weakened_fiftyshot = foo_100_weakened_fiftyshot.weaken(2).update(y.tail(50), X.bottom(50));

    Matrix Xw = X;
    Xw.top(50) *= 0.5;
    Vector yw = y;
    yw.head(50) *= 0.5;
    BayesianLinear foo_100_weakened_direct = foo.update(yw, Xw);

    print_model(foo_100_weakened_direct);
    print_model(foo_100_weakened_fiftyshot);

    Vector betahat_weakhalf = Xw.solveLeastSquares(yw);
    std::cout << "OLS with pre-scaled prior data:\n";
    print_OLS("OLS (scale-weakened first half)", betahat_weakhalf, Xw, yw);
}
