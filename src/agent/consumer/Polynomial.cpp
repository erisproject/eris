#include "agent/consumer/Polynomial.hpp"

// A consumer who has quadratic utility of the form:
// AX + BX^2
// where A and B are rows (arrays) of k coefficients, X is a kx1 column vector
// of good quantities, and X^2 is a column vector of squared good quantities.
// If A or B are smaller than k, 0 is used as the coefficients from the end
// through k.

Polynomial::Polynomial(double offset) : offset(offset) {}

Polynomial::Polynomial(std::map<eris_id_t, std::vector<double>> coef, double offset) : offset(offset), coef(coef) {}

std::vector<double> &Polynomial::operator[](eris_id_t gid) {
    return coef[gid];
}

double Polynomial::utility(Bundle b) {
    double u = offset;
    for (std::pair<eris_id_t, std::vector<double>> c : coef) {
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
double Polynomial::d(Bundle b, eris_id_t g) {
    double up = 0.0;
    double q = b[g];
    auto c = coef[g];
    double qpow = 1.0;
    double i = 1.0;
    for (double alpha : c) {
        up += (i++)*alpha*qpow;
        qpow *= q;
    }

    return up;
}

double Polynomial::d2(Bundle b, eris_id_t g1, eris_id_t g2) {
    double upp = 0.0;
    // Separable polynomial utility has no iteration terms, so Hessian is diagonal
    if (g1 != g2) return upp;

    double q = b[g1];
    auto c = coef[g1];
    double qpow = 1.0;
    double i = 2.0;
    bool first = true;
    for (double alpha : c) {
        if (first) { first = false; continue; }
        upp += (i-1)*(i++)*alpha*qpow;
        qpow *= q;
    }

    return upp;
}

// Override Consumer's hessian function: since off-diagonals of the Hessian are
// 0 for this class, we can skip those calculations.
std::map<eris_id_t, std::map<eris_id_t, double>> Polynomial::hessian(std::vector<eris_id_t> goods, Bundle b) {
    std::map<eris_id_t, std::map<eris_id_t, double>> H;

    for (auto g1 : goods) {
        for (auto g2 : goods) {
            H[g1][g2] = g1 == g2 ? d2(b, g1, g2) : 0;
        }
    }

    return H;
}
