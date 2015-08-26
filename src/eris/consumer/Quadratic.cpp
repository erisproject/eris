#include <eris/consumer/Quadratic.hpp>
#include <utility>
#include <unordered_map>

namespace eris { namespace consumer {

Quadratic::Quadratic(double offset) : offset(offset) {}
Quadratic::Quadratic(std::map<eris_id_t, double> linear, double offset) : offset(offset), linear(linear) {}

// Constant
double& Quadratic::coef() {
    return offset;
}
double Quadratic::coef() const {
    return offset;
}
// Linear term
double& Quadratic::coef(eris_id_t g) {
    return linear[g];
}
double Quadratic::coef(eris_id_t g) const {
    return linear.count(g) ? linear.at(g) : 0.0;
}
// Quadratic term
double& Quadratic::coef(eris_id_t g1, eris_id_t g2) {
    // quad only stores elements with g1 <= g2, so swap if necessary
    if (g1 > g2) std::swap(g1, g2);
    return quad[g1][g2];
}
double Quadratic::coef(eris_id_t g1, eris_id_t g2) const {
    if (g1 > g2) std::swap(g1, g2);
    return quad.count(g1) && quad.at(g1).count(g2) ? quad.at(g1).at(g2) : 0.0;
}

double Quadratic::utility(const BundleNegative &b) const {
    double u = offset;

    for (auto g1it = b.begin(); g1it != b.end(); ++g1it) {
        if (linear.count(g1it->first))
            u += linear.at(g1it->first) * g1it->second;

        // For quadratic terms, only add starting at the current element (otherwise we'd end up
        // adding both c*g1*g2 and c*g2*g1, which isn't wanted.
        for (auto g2it = g1it; g2it != b.end(); ++g2it)
            u += (coef(g1it->first, g2it->first)) * (g1it->second) * (g2it->second);
    }

    return u;
}

double Quadratic::d(const BundleNegative &b, eris_id_t g) const {
    double up = linear.count(g) ? linear.at(g) : 0.0;

    for (auto g2 : b) {
        double c = coef(g, g2.first);
        if (c != 0) {
            double _u = c * b[g2.first];
            if (g == g2.first) _u *= 2.0; // Squared term has the extra 2 coefficient
            up += _u;
        }
    }

    return up;
}

// The second derivative is really easy: it's just the coefficient for off-diagonals, and double the
// coefficient for diagonals; it doesn't depend on the bundle at all.
double Quadratic::d2(const BundleNegative&, eris_id_t g1, eris_id_t g2) const {
    double upp = coef(g1, g2);
    if (g1 == g2) upp *= 2.0;
    return upp;
}

} }
