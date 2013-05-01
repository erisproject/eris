#include "consumer/Quadratic.hpp"
#include <utility>
#include <vector>

namespace eris { namespace consumer {

Quadratic::Quadratic() {}
Quadratic::Quadratic(double offset, std::map<eris_id_t, double> linear) : offset(offset), linear(linear) {}

double Quadratic::utility(Bundle b) {
    double u = offset;

    for (auto g1 : b) {
        double q1 = g1.second;
        u += linear[g1.first] * q1;

        for (auto g2 : b) {
            u += getQuadCoef(g1.first, g2.first) * g1.second * g2.second;
        }
    }

    return u;
}

double Quadratic::d(Bundle b, eris_id_t g) {
    double up = linear[g];

    for (auto g2 : b) {
        double _u = getQuadCoef(g, g2.first);
        if (g == g2.first) _u *= 2.0; // Squared term has the extra 2 coefficient
        up += _u;
    }

    return up;
}

// The second derivative is really easy: it's just the coefficient (double for
// Hessian diagonals):
double Quadratic::d2(Bundle b, eris_id_t g1, eris_id_t g2) {
    double upp = getQuadCoef(g1, g2);
    if (g1 == g2) upp *= 2.0;
    return upp;
}

} }
