#include <iostream>
#include <eris/belief/BayesianLinearRestricted.hpp>
#include <Eigen/Cholesky>
#include <Eigen/QR>
#include <list>

using namespace eris::belief;
using namespace Eigen;

int main() {

    VectorXd beta(3);
    beta << 10, 2, -5;

    MatrixXd V(3,3);
    V << 1.7, 0.25, 0.4,
         0.25,  5.5, 1,
         0.4, 1, 8;
    double s2 = 2.0;
    double n = 10;

    BayesianLinearRestricted model(beta, s2, V.fullPivHouseholderQr().inverse(), n);
    model.draw_mode = BayesianLinearRestricted::DrawMode::Gibbs;

    std::cout << "s2=" << model.s2() << ", V:\n" << model.Vinvinv() << "\n";
    MatrixXd X(5, 3);
    X <<
      1, 4, 0,
      2, 2, 2,
     -3, 5, 17,
      1, 1, 1,
      8, 0, 6;

    VectorXd yposterior = X * model.beta();
    std::cout << "X beta_post: " << yposterior.transpose() << "\n";
    unsigned ndraws = 100000;

    VectorXd s2idraws(ndraws);
    MatrixXd betadraws(model.K(), ndraws);
    MatrixXd ypred(X.rows(), ndraws);
    MatrixXd gammadraws(model.K(), ndraws);
    MatrixXd gammaL = model.VinvLLT().matrixU().solve(MatrixXd::Identity(model.K(), model.K()));
    for (unsigned i = 0; i < ndraws; i++) {
        model.discard();
        ypred.col(i) = model.predict(X, 1);
        betadraws.col(i) = model.lastDraw().head(3);
        s2idraws[i] = 1.0 / model.lastDraw()[3];
        gammadraws.col(i) = BayesianLinear::multivariateT(model.beta(), model.n(), gammaL, std::sqrt(model.s2()));
    }
    VectorXd means = ypred.rowwise().mean();
    VectorXd var = ((ypred.colwise() - means).rowwise().squaredNorm() / (ndraws-1));

    std::cout << "s^-2 draws mean: " << s2idraws.mean() << " (expect " << 1/s2 << ")\n";
    std::cout << "s^-2 draws var:  " << (s2idraws.array() - s2idraws.mean()).matrix().squaredNorm() / (ndraws-1) <<
        " (expect " << 2/(n*s2*s2) << ")\n";

    VectorXd betadrawmeans = betadraws.rowwise().mean();
    MatrixXd beta_demeaned = betadraws.colwise() - betadrawmeans;
    MatrixXd betadrawvar = beta_demeaned * beta_demeaned.transpose() / (ndraws-1);
    std::cout << "betamean: " << betadrawmeans.transpose() << " (expect " << model.beta().transpose() << ")\n";
    std::cout << "betavar:\n" << betadrawvar << "\nexpected var:\n" << model.n() * model.s2() / (model.n()-2) * model.Vinvinv()
        << "\n";

    VectorXd gammadrawmeans = gammadraws.rowwise().mean();
    MatrixXd gamma_demeaned = gammadraws.colwise() - gammadrawmeans;
    MatrixXd gammadrawvar = gamma_demeaned * gamma_demeaned.transpose() / (ndraws-1);
    std::cout << "gammamean: " << gammadrawmeans.transpose() << " (expect " << model.beta().transpose() << ")\n";
    std::cout << "gammavar:\n" << gammadrawvar << "\nexpected var:\n" << model.n() * model.s2() / (model.n()-2) * model.Vinvinv()
        << "\n";

    std::cout << "y* means: " << means.transpose() << "\n";
    std::cout << "y* varis: " << var.transpose() << "\n";

    auto mv = model.predictVariance(X, 10000);
    std::cout << "y*pv means: " << mv.col(0).transpose() << "\n";
    std::cout << "y*pv vars:  " << mv.col(1).transpose() << "\n";

    MatrixXd L = (model.s2() * (MatrixXd::Identity(X.rows(), X.rows()) + X * V * X.transpose())).llt().matrixL();
    MatrixXd yshould(X.rows(), ndraws);
    for (unsigned i = 0; i < ndraws; i++) {
        yshould.col(i) = BayesianLinear::multivariateT(
                X*model.beta(),
                model.n(),
                L);
    }

    VectorXd shouldmeans = yshould.rowwise().mean();
    VectorXd shouldvar = ((yshould.colwise() - means).rowwise().squaredNorm() / (ndraws-1));
    std::cout << "should means: " << shouldmeans.transpose() << "\n";
    std::cout << "should varis: " << shouldvar.transpose() << "\n";

}
