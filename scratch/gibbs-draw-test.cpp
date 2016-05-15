#include <iostream>
#include <eris/belief/BayesianLinearRestricted.hpp>
#include <Eigen/Cholesky>
#include <Eigen/QR>
#include <eris/random/rng.hpp>
#include <list>

using namespace eris::belief;
using namespace Eigen;

int main() {

    VectorXd beta(5);
    beta << -72.458353, -3.596262, 4.155980, -0.575780, 2.966034;

    std::cerr << "seed: " << eris::random::seed() << "\n";

    double s2 = 183.837366;
    double n = 323;
    MatrixXd V(5,5);
    V(0,0) = 127.962;
    V(1,0) = 4.47105;  V(1,1) = 0.22535;
    V(2,0) = -5.14075; V(2,1) = -0.205758; V(2,2) = 0.220634;
    V(3,0) = 0.464975; V(3,1) = 0.0117565; V(3,2) = -0.0200646; V(3,3) = 0.00535619;
    V(4,0) = -22.2615; V(4,1) = -0.977851; V(4,2) = 0.953113;   V(4,3) = -0.100887; V(4,4) = 18.6389;
    for (int i = 1; i < 5; i++) for (int j = 0; j < i; j++) V(j,i) = V(i,j);

    V /= s2;

    BayesianLinear unrestricted(beta, s2, V.fullPivHouseholderQr().inverse(), n);
    BayesianLinearRestricted unrestricted2(beta, s2, V.fullPivHouseholderQr().inverse(), n);

    BayesianLinearRestricted model(beta, s2, V.fullPivHouseholderQr().inverse(), n);
    model.restrict(1) <= -0.05;
    model.restrict(2) >= 0.0;
    model.restrict(4) <= -1.0;
    model.draw_gibbs_burnin = 100;
    model.draw_gibbs_thinning = 2;

    std::cout << model;

    std::cout << "s2=" << model.s2() << ", s2*V:\n" << s2*model.Vinvinv() << "\n";

    constexpr size_t draws = 10000;
    MatrixXd beta_rej(5+1, draws), beta_gibbs(5+1, draws), beta_unrest(5+1, draws), beta_unrest2(5+1,draws);

    model.draw_mode = BayesianLinearRestricted::DrawMode::Rejection;
    for (unsigned i = 0; i < draws; i++) {
        beta_rej.col(i) = model.drawRejection(draws);
    }

    std::cerr << draws << " draws: rejection discards: " << model.draw_rejection_discards << "\n"
        << "rejection successes: " << model.draw_rejection_success << "\n";

    model.draw_mode = BayesianLinearRestricted::DrawMode::Gibbs;
    for (unsigned i = 0; i < draws; i++) {
        beta_gibbs.col(i) = model.draw();
        beta_unrest.col(i) = unrestricted.draw();
        beta_unrest2.col(i) = unrestricted2.drawGibbs();
    }

    std::cerr << draws << " draws: rejection discards: " << model.draw_rejection_discards << "\n"
        << "rejection successes: " << model.draw_rejection_success << "\n";

    std::cerr << "means:\nrejection: " << beta_rej.rowwise().mean().transpose() <<
                       "\ngibbs:     " << beta_gibbs.rowwise().mean().transpose() <<
                       "\nunrest:    " << beta_unrest.rowwise().mean().transpose() <<
                       "\nunrest(g): " << beta_unrest2.rowwise().mean().transpose() <<
                       "\n\n";

    std::cerr << "min:\nrejection: " << beta_rej.rowwise().minCoeff().transpose() <<
                     "\ngibbs:     " << beta_gibbs.rowwise().minCoeff().transpose() <<
                     "\nunrest:    " << beta_unrest.rowwise().minCoeff().transpose() <<
                     "\nunrest(g): " << beta_unrest2.rowwise().minCoeff().transpose() <<
                     "\n\n";

    std::cerr << "max:\nrejection: " << beta_rej.rowwise().maxCoeff().transpose() <<
                     "\ngibbs:     " << beta_gibbs.rowwise().maxCoeff().transpose() <<
                     "\nunrest:    " << beta_unrest.rowwise().maxCoeff().transpose() <<
                     "\nunrest(g): " << beta_unrest2.rowwise().maxCoeff().transpose() <<
                     "\n\n";
}
