#include <eris/consumer/Polynomial.hpp>
#include <utility>

namespace eris { namespace consumer {

// A consumer who has quadratic utility of the form:
// AX + BX^2
// where A and B are rows (arrays) of k coefficients, X is a kx1 column vector
// of good quantities, and X^2 is a column vector of squared good quantities.
// If A or B are smaller than k, 0 is used as the coefficients from the end
// through k.

Polynomial::Polynomial(double offset) : offset(offset) {}

Polynomial::Polynomial(std::map<eris_id_t, std::vector<double>> coef, double offset) : offset(offset), coefficients(coef) {}

double& Polynomial::coef(MemberID g, unsigned int n) {
    if (n == 0) return coef();

    if (coefficients[g].size() < n)
        coefficients[g].resize(n);

    return coefficients[g][n-1];
}

double Polynomial::coef(MemberID g, unsigned int n) const {
    if (n == 0) return coef();

    return coefficients.count(g) and coefficients.at(g).size() > n
        ? coefficients.at(g).at(n)
        : 0.0;
}

double& Polynomial::coef() {
    return offset;
}

double Polynomial::coef() const {
    return offset;
}

double Polynomial::utility(const BundleNegative &b) const {
    double u = offset;
    for (auto c : coefficients) {
        double q = b[c.first];
        double qpow = 1.0;
        if (q != 0) {
            for (double alpha : c.second) {
                qpow *= q;
                u += alpha*qpow;
            }
        }
    }

    return u;
}

// Calculate the derivative for good g.  Since utility is separable, we only
// need to use the coefficients for good g to get the value of the derivative
double Polynomial::d(const BundleNegative &b, MemberID g) const {
    if (!coefficients.count(g)) return 0.0;
    auto c = coefficients.at(g);
    double up = 0.0;
    double q = b[g];
    double qpow = 1.0;
    double exponent = 1.0;
    for (double alpha : c) {
        up += (exponent++)*alpha*qpow;
        qpow *= q;
    }

    return up;
}

double Polynomial::d2(const BundleNegative &b, MemberID g1, MemberID g2) const {
    double upp = 0.0;

    // Separable polynomial utility has no iteration terms, so Hessian is diagonal; thus we can just
    // return 0 right away if we're looking for an off-diagonal element.  We can also return it
    // right away if g1 doesn't have any coefficients at all.
    if (g1 != g2 || !coefficients.count(g1)) return upp;

    double q = b[g1];
    auto c = coefficients.at(g1);
    double qpow = 1.0;
    double exponent = 2.0;
    bool first = true;
    for (double alpha : c) {
        if (first) { first = false; continue; }
        upp += (exponent-1)*exponent*alpha*qpow;
        qpow *= q;
        ++exponent;
    }

    return upp;
}

// Override Consumer's hessian function: since off-diagonals of the Hessian are
// 0 for this class, we can skip those calculations.
std::map<eris_id_t, std::map<eris_id_t, double>>
Polynomial::hessian(const std::vector<eris_id_t> &goods, const BundleNegative &b) const {
    std::map<eris_id_t, std::map<eris_id_t, double>> H;

    for (auto g1 : goods) {
        for (auto g2 : goods) {
            H[g1][g2] = g1 == g2 ? d2(b, g1, g2) : 0;
        }
    }

    return H;
}

} }
