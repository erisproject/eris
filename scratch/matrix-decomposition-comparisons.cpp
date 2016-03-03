#include <Eigen/Cholesky>
#include <Eigen/SVD>
#include <Eigen/QR>
#include <Eigen/LU>
#include <eris/random/distribution.hpp>
#include <iostream>
#include <map>
#include <cstdlib>

using namespace Eigen;
using namespace eris;

#define print_OLS(name, beta, X, y) std::cout << name << ":\n" << \
    u8"    β^: " << beta.transpose() << "\n" << \
    u8"    σ^²: " << (y - X * beta).squaredNorm() / X.rows() << "\n" << \
    u8"    X'X: " << (X.transpose() * X).format(IOFormat(StreamPrecision, 0, " ", "\n         ")) << "\n" << \
    u8"    (X'X)^-1: " << (X.transpose() * X).inverse().format(IOFormat(StreamPrecision, 0, " ", "\n              ")) << "\n"


// Returns the amount by which coefficients in m2 differ from those in m1, relative to the value of
// the coefficient in m1.
MatrixXd reldiff(const MatrixXd &m1, const MatrixXd &m2) {
    return (m2 - m1).cwiseQuotient(m1.cwiseAbs());
}

int main() {

    constexpr unsigned n = 20, K = 4;
    MatrixXd Z(n, 20);
    MatrixXd X(n, K);
    VectorXd beta0(K);
    beta0 << -1.4, .77, .01, .45;
    for (unsigned c = 0; c < Z.cols(); c++) {
        for (unsigned r = 0; r < n; r++) {
            Z(r,c) = random::rstdnorm();
        }
    }
    X.col(0).setConstant(1);
    X.col(1) = Z.col(0) + 190*Z.col(2);
    X.col(2) = Z.col(3) + 220*Z.col(2);
    X.col(3) = Z.col(5) - 97.5*Z.col(2);
    VectorXd u(X.rows());
    for (unsigned r = 0; r < n; r++) {
        u[r] = 2.75*Random::rstdnorm();
    }

    MatrixXd y = X * beta0 + u;

    JacobiSVD<MatrixXd> svdX = X.jacobiSvd(ComputeThinU | ComputeThinV);

    VectorXd betahat = svdX.solve(y);
    double svdSSR = (y - X*betahat).squaredNorm();

    print_OLS("OLS (via SVD)", betahat, X, y);

    MatrixXd Xty = X.transpose() * y;
    MatrixXd XtXorig = (X.transpose() * X).selfadjointView<Lower>();
    auto qr = X.fullPivHouseholderQr();
    //std::cout << "transpositions:\n" << qr.colsPermutation() << "\n";
    MatrixXd RPi = qr.matrixQR().triangularView<Upper>();
    RPi *= qr.colsPermutation().inverse();
    MatrixXd XtX = RPi.transpose() * RPi;

    MatrixXd XtXi = XtX.fullPivHouseholderQr().inverse();

    std::cout << "xtx orig: \n" << XtXorig << "\nxtx qr:\n" << XtX << "\n";

    JacobiSVD<MatrixXd> svdXtX = XtX.jacobiSvd();

    VectorXd random(K);
    for (int i = 0; i < random.size(); i++) {
        random[i] = Random::rstdnorm();
    }

    std::cout << "CPQR SSR - SVD SSR: "   << (y - X * XtX.colPivHouseholderQr().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "FPQR SSR - SVD SSR: "   << (y - X * XtX.fullPivHouseholderQr().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "PPLU SSR - SVD SSR: "   << (y - X * XtX.partialPivLu().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "FPLU SSR - SVD SSR: "   << (y - X * XtX.fullPivLu().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "LLT SSR - SVD SSR: "    << (y - X * XtX.selfadjointView<Lower>().llt().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "LDLT SSR - SVD SSR: "   << (y - X * XtX.selfadjointView<Lower>().ldlt().solve(Xty)).squaredNorm() - svdSSR << "\n";
    std::cout << "SVDXtX SSR - SVD SSR: " << (y - X * XtX.jacobiSvd(ComputeThinU | ComputeThinV).solve(Xty)).squaredNorm() - svdSSR << "\n";

    std::map<std::string, MatrixXd> L;
    L.emplace("XtX->FPQR->inverse->LLT", XtX.fullPivHouseholderQr().inverse().llt().matrixL());
    L.emplace("XtX->FPLU->inverse->LLT", XtX.fullPivLu().inverse().llt().matrixL());
    L.emplace("X->SVD", svdX.matrixV() * svdX.singularValues().cwiseInverse().asDiagonal());
    L.emplace("XtX->LLT->FPQR->inverse->T", FullPivHouseholderQR<MatrixXd>(XtX.selfadjointView<Lower>().llt().matrixL()).inverse().transpose());
    // This one is wrong, but including it for comparison:
    L.emplace("XtX->LLT->matrixL->solve(I)", XtX.llt().matrixL().solve(MatrixXd::Identity(X.cols(), X.cols())));
    L.emplace("XtX->LLT->matrixU->solve(I)", XtX.llt().matrixU().solve(MatrixXd::Identity(X.cols(), X.cols())));
//    L.emplace("XtX->SVD", svdXtX 

    for (const auto &l_impl : L) {
        std::cout << "L for " << l_impl.first << ":\n" << l_impl.second << "\n\n";
    }

    // Reuse the same set of random N(0,1) draws
    constexpr unsigned ndraws = 100000;
    MatrixXd rn01(K, ndraws);
    for (unsigned c = 0; c < rn01.cols(); c++) for (unsigned r = 0; r < rn01.rows(); r++) {
        rn01(r,c) = Random::rstdnorm();
    }

    std::map<std::string, MatrixXd> draws;
    for (const auto &l_impl : L) {
        auto elem = draws.emplace(l_impl.first, MatrixXd::Constant(X.cols(), ndraws, std::numeric_limits<double>::quiet_NaN()));
        auto &d = elem.first->second;
        for (unsigned i = 0; i < ndraws; i++) {
            d.col(i) = l_impl.second * rn01.col(i);
        }
    }

    for (const auto &l : L) {
        const MatrixXd &d = draws.at(l.first);
        VectorXd means = d.rowwise().mean();
        MatrixXd centered = d.colwise() - d.rowwise().mean();
        MatrixXd cov = (centered * centered.transpose()) / (centered.cols()-1);
        std::cout << "results for " << l.first << ":\n";
        std::cout << "mean: " << means.transpose() << "\ncovariance:\n" << cov << "\n";
        std::cout << "relative cov error (i.e. relative diff from (X'X)^-1):\n" << reldiff(XtXi, cov) << "\n\n";
    }

    std::ostringstream R;
    R << "Rscript -e 'require(mvtnorm, quietly=T); X <- matrix(ncol=" << K << ", c(";
    R.precision(std::numeric_limits<double>::max_digits10);
    for (unsigned c = 0; c < K; c++) {
        for (unsigned r = 0; r < n; r++) {
            if (r > 0 or c > 0) R << ",";
            R << X(r,c);
        }
    }
    R << ")); mcov <- solve(crossprod(X)); ";
    R << "zz <- rmvnorm(n=" << ndraws << ", mean=rep(0," << K << "), sigma=mcov); ";
    R << "cat(\"Means: \"); print(colMeans(zz)); ";
    R << "cat(\"Relative cov error:\\n\"); print((mcov - var(zz)) / abs(mcov))'";
    std::cout << "Results from R's mvtnorm package:\n";
    std::system(R.str().c_str());
    std::cout << "\n\n";


    for (const auto &l : L) {
        MatrixXd XtX2 = (l.second * l.second.transpose()).fullPivHouseholderQr().inverse();
        std::cout << l.first << ": reconstructed XtX - original XtX:\n" << XtX2 - XtX << "\n\n";
    }

}
