#include "agent/consumer/Quadratic.hpp"

// A consumer who has quadratic utility of the form:
// AX + BX^2
// where A and B are rows (arrays) of k coefficients, X is a kx1 column vector
// of good quantities, and X^2 is a column vector of squared good quantities.
// If A or B are smaller than k, 0 is used as the coefficients from the end
// through k.

Quadratic::Quadratic() {}

Quadratic::Quadratic(std::map<eris_id_t, std::vector<double>> coefmap) : Quadratic() {
    for (std::pair<eris_id_t, std::vector<double>> coefpair : coefmap)
        setCoefs(coefpair.first, coefpair.second);
}

void Quadratic::setCoefs(eris_id_t gid, std::vector<double> c) {
    coef.insert(std::make_pair(gid, c));
}

void Quadratic::setCoefs(eris_id_t gid, std::initializer_list<double> c) {
    setCoefs(gid, std::vector<double>(c.begin(), c.end()));
}

double Quadratic::utility(Bundle b) {
    double u = 0.0;
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
