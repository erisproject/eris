#include <eris/consumer/CobbDouglas.hpp>
#include <limits>
#include <utility>
#include <cmath>

namespace eris { namespace consumer {

CobbDouglas::CobbDouglas(double c) : constant(c) {}
CobbDouglas::CobbDouglas(
        id_t g1,
        double exp1,
        id_t g2,
        double exp2,
        id_t g3,
        double exp3,
        double c
        ) : constant(c) {
    if (g1 != 0) exponents[g1] = exp1;
    if (g2 != 0) exponents[g2] = exp2;
    if (g3 != 0) exponents[g3] = exp3;
}
CobbDouglas::CobbDouglas(const std::unordered_map<id_t, double> &exps, double c) : constant(c), exponents(exps) {}

// Constant
double& CobbDouglas::coef() {
    return constant;
}
double CobbDouglas::coef() const {
    return constant;
}
// Linear term
double& CobbDouglas::exp(MemberID g) {
    return exponents[g];
}
double CobbDouglas::exp(MemberID g) const {
    return exponents.count(g) ? exponents.at(g) : 0.0;
}

double CobbDouglas::power(double val, double exp) const {
    if (exp == 0) return 1.0;

    if (val == 0) {
        // If we have a 0 value and a positive exponent, total utility is 0
        if (exp > 0) return 0.0;
        // Otherwise (a negative exponent, since 0 was already skipped, above) we get infinity
        else return std::numeric_limits<double>::infinity();
    }

    return (exp == 1) ? val : pow(val, exp);
}

double CobbDouglas::utility(const BundleNegative &b) const {
    double u = constant;

    for (auto e : exponents) {
        if (u == 0 or u == std::numeric_limits<double>::infinity()) return u;
        if (e.second == 0) continue;

        double val = b[e.first];

        u *= power(val, e.second);
    }

    return u;
}

double CobbDouglas::d(const BundleNegative &b, MemberID g) const {
    // Short-circuit cases resulting in a 0 derivative
    if (!exponents.count(g) or exponents.at(g) == 0)
        return 0.0;

    double grad = constant;
    for (auto e : exponents) {
        if (grad == 0 or grad == std::numeric_limits<double>::infinity()) return grad;

        double exp = e.second;
        double val = b[e.first];

        if (e.first == g) {
            grad *= exp;
            exp -= 1.0;
        }

        grad *= power(val, exp);
    }
    return grad;
}
double CobbDouglas::d2(const BundleNegative &b, MemberID g1, MemberID g2) const {
    // Short-circuit all the things that always result in a 0 second derivative
    if (!exponents.count(g1) or !exponents.count(g2) or exponents.at(g1) == 0 or exponents.at(g2) == 0)
        return 0.0;

    double h = constant;
    for (auto e : exponents) {
        if (h == 0 or h == std::numeric_limits<double>::infinity()) return h;

        double exp = e.second;
        double val = b[e.first];

        if (e.first == g1) {
            h *= exp;
            exp -= 1.0;
        }
        if (e.first == g2) {
            if (exp == 0) return 0.0;
            h *= exp;
            exp -= 1.0;
        }

        h *= power(val, exp);
    }
    return h;
}

} }
