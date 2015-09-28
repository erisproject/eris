#include <iostream>
#include <eris/belief/BayesianLinear.hpp>
#include <Eigen/Cholesky>
#include <eris/Random.hpp>
#include <list>
#include <ctime>
extern "C" {
#include <unistd.h>
#include <sys/wait.h>
#include <sys/times.h>
}

using namespace eris::belief;
using namespace Eigen;

std::string matrix_to_Rc(const Ref<const MatrixXd> &m) {
    std::ostringstream str;
    str << "c(";
    bool first = true;
    for (unsigned c = 0; c < m.cols(); c++) for (unsigned r = 0; r < m.rows(); r++) {
        if (first) first = false;
        else str << ",";
        str << m(r,c);
    }
    str << ")";
    return str.str();
}

int main() {

    MatrixXd sigma(5,5);
    sigma << 5,   4,   3,   2, 1,
             4,   6, 0.5,   0, 1,
             3, 0.5,   7,   0, 1.5,
             2,   0,   0,   8, 0.1,
             1,   1, 1.5, 0.1, 9;

    MatrixXd L = sigma.llt().matrixL();

    VectorXd mu(5);
    mu << 8, 1, 1, 150, 888;

    double df = 8;

    std::cout << "mu: " << mu.transpose() << "\n\nsigma:\n" << sigma << "\ndf: " << df << "\n\nmvt:\n====\n";

    clock_t start = std::clock();

    MatrixXd draws(5, 1000000);
    for (unsigned i = 0; i < draws.cols(); i++) {
        draws.col(i) = BayesianLinear::multivariateT(mu, df, L);
    }

    clock_t end = std::clock();

    VectorXd means = draws.rowwise().mean();
    VectorXd var = ((draws.colwise() - means).rowwise().squaredNorm() / (draws.cols()-1));

    std::cout << "means:\n" << means.transpose() << "\n\nvariance:\n" << var.transpose() << "\n\n";

    double s = (double)(end - start) / CLOCKS_PER_SEC;
    std::cout << "Elapsed: " << s << " s (" << std::lround(draws.cols() / s) << " draws/s)\n";

    std::ostringstream Rmvtnorm, Rmnormt;
#define BOTH(WHATEVER) Rmvtnorm << WHATEVER; Rmnormt << WHATEVER
    Rmvtnorm << "require(mvtnorm,quietly=T);";
    Rmnormt << "require(mnormt,quietly=T);set.seed(" << (int) eris::Random::rng()() << ");";
    BOTH("S<-matrix(nrow=" << sigma.rows() << "," << matrix_to_Rc(sigma) << ");" <<
            "M<-" << matrix_to_Rc(mu) << ";");
    BOTH("t0<-proc.time()[3];");
    Rmvtnorm << "d<-M+t(rmvt(" << draws.cols() << ",S," << df << "));";
    Rmnormt << "d<-t(rmt(" << draws.cols() << ",M,S," << df << "));";
    BOTH("tZ<-proc.time()[3];");
    BOTH("m<-rowMeans(d);"
            << "v<-apply(d,1,function(x)var(x));"
            << "z<-" << matrix_to_Rc(means) << ";Z<-" << matrix_to_Rc(var) << ";"
            << R"(cat("Means:",m,"\nVariances:",v,"\nEris minus )");
    Rmvtnorm << "mvtnorm"; Rmnormt << "mnormt";
    BOTH(R"( means:",z-m,"\nEris minus )");
    Rmvtnorm << "mvtnorm"; Rmnormt << "mnormt";
    BOTH(R"( variances:",Z-v,"\n\n");)");
    BOTH(R"(cat("Elapsed:",tZ-t0,"s (",ncol(d)/(tZ-t0)," draws/s)\n\n"))");

    std::list<std::pair<std::string, std::string>> commands{
        {"mvtnorm", Rmvtnorm.str()},
        {"mnormt", Rmnormt.str()}};


    for (auto &R : commands) {
        std::cout << "\n\nRunning R with package " << R.first << " for comparison:\n====\n";
        pid_t child = fork();
        if (child == 0) {
            // Child
            execlp("R", "R", "--slave", "-e", R.second.c_str(), nullptr);

            // Getting here means exec failed!
            std::cerr << "Failed to execute R: " << std::strerror(errno) << ".  Tried to execute:\nR --slave -e '" << R.second << "'\n";

            return errno;
        }
        else if (child == -1) {
            std::cerr << "Fork failed: " << std::strerror(errno) << "\n";
            return errno;
        }
        else {
            wait(nullptr);
        }
    }
}
