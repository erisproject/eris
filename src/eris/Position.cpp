#include "eris/Position.hpp"
#include <cmath>

namespace eris {

Position::Position(std::initializer_list<double> position) : pos_(position) {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(const size_t &dimensions) : pos_(dimensions, 0) {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(const Position &pos) : pos_(pos.dimensions, 0) {
    *this = pos;
}

double Position::distance(const Position &other) const {
    requireSameDimensions(other, "Position::distance");

    if (dimensions == 1)
        return fabs(at(0) - other[0]);

    if (dimensions == 2)
        return hypot(at(0) - other[0], at(1) - other[1]);

    // NB: could protect this against underflow/overflow by adapting the hypot algorithm to 3+
    // dimensions; see http://en.wikipedia.org/wiki/Hypot
    double dsq = 0;
    for (int i = 0; i < dimensions; ++i)
        dsq += pow(at(i) - other[i], 2);
    return sqrt(dsq);
}

double Position::length() const {
    if (dimensions == 1) return fabs(at(0));
    if (dimensions == 2) return hypot(at(0), at(1));

    // NB: could protect this against underflow/overflow by adapting the hypot algorithm to 3+
    // dimensions; see http://en.wikipedia.org/wiki/Hypot
    double lsq = 0.0;
    for (auto &d : pos_)
        lsq += d*d;
    return sqrt(lsq);
}

Position Position::mean(const Position &other, const double &weight) const {
    requireSameDimensions(other, "Position::mean");

    Position result(dimensions);
    double our_weight = 1.0-weight;

    for (int i = 0; i < dimensions; i++)
        result[i] = our_weight*at(i) + weight*other[i];

    return result;
}

Position Position::subdimensions(std::initializer_list<double> dims) const {
    Position p(dims.size());
    size_t i = 0;
    for (const double &d : dims) {
        if (d >= dimensions)
            throw std::out_of_range("Invalid subdimensions call: attempt to use invalid dimension");
        p[i++] = operator[](d);
    }
    return p;
}

Position& Position::operator=(const Position &new_pos) {
    requireSameDimensions(new_pos, "Position::operator=");

    for (int i = 0; i < dimensions; i++)
        at(i) = new_pos[i];

    return *this;
}

bool Position::operator==(const Position &other) const {
    requireSameDimensions(other, "Position::operator==");

    for (int i = 0; i < dimensions; i++)
        if (at(i) != other[i]) return false;

    return true;
}

bool Position::operator!=(const Position &other) const {
    requireSameDimensions(other, "Position::operator!=");
    return !(*this == other);
}

// Adding two positions together adds the underlying position values together.
Position Position::operator+(const Position &add) const {
    requireSameDimensions(add, "Position::operator+");
    return Position(*this) += add;
}

// Mutator version of addition
Position& Position::operator+=(const Position &add) {
    requireSameDimensions(add, "Position::operator+=");
    for (int i = 0; i < dimensions; i++)
        at(i) += add[i];

    return *this;
}

// Subtracting a position from another subtracts the individual elements.
Position Position::operator-(const Position &subtract) const {
    requireSameDimensions(subtract, "Position::operator-");
    return Position(*this) -= subtract;
}

// Mutator version of subtraction.
Position& Position::operator-=(const Position &subtract) {
    requireSameDimensions(subtract, "Position::operator-=");
    for (int i = 0; i < dimensions; i++)
        at(i) -= subtract[i];

    return *this;
}

// Unary negation of a position is unary negation of each dimension value.
Position Position::operator-() const noexcept {
    return *this * -1.0;
}

// A position can be scaled by a constant, which scales each dimension value.
Position Position::operator*(const double &scale) const noexcept {
    return Position(*this) *= scale;
}

// Mutator version of scaling.
Position& Position::operator*=(const double &scale) noexcept {
    for (int i = 0; i < dimensions; i++)
        at(i) *= scale;

    return *this;
}

// Scales each dimension value by 1/d
Position Position::operator/(const double &inv_scale) const noexcept {
    return Position(*this) *= (1.0/inv_scale);
}

// Mutator version of division scaling.
Position& Position::operator/=(const double &inv_scale) noexcept {
    return operator *= (1.0/inv_scale);
}

std::ostream& operator<<(std::ostream &os, const Position &p) {
    os << "Position[" << p[0];
    for (int i = 1; i < p.dimensions; i++)
        os << ", " << p[i];
    os << "]";
    return os;
}

}
