#include "eris/Position.hpp"
#include <cmath>

namespace eris {

Position::Position(std::initializer_list<double> position) : pos_(position) {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(const int &dimensions) : pos_(dimensions, 0) {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(const Position &pos) : pos_(pos.dimensions, 0) {
    *this = pos;
}

double Position::distance(const Position &other) const {
    requireSameDimensions(other, "Position::distance");

    if (dimensions == 1)
        return fabs(operator[](0) - other[0]);

    if (dimensions == 2)
        return hypot(operator[](0) - other[0], operator[](1) - other[1]);

    double dsq = 0;
    for (int i = 0; i < dimensions; ++i)
        dsq += pow(operator[](i) - other[i], 2);
    return sqrt(dsq);
}

Position Position::mean(const Position &other, const double &weight) const {
    requireSameDimensions(other, "Position::mean");

    Position result(dimensions);
    double our_weight = 1.0-weight;

    for (int i = 0; i < dimensions; i++)
        result[i] = our_weight*operator[](i) + weight*other[i];

    return result;
}

Position& Position::operator=(const Position &new_pos) {
    requireSameDimensions(new_pos, "Position::operator=");

    for (int i = 0; i < dimensions; i++)
        operator[](i) = new_pos[i];

    return *this;
}

// Adding two positions together adds the underlying position values together.
Position Position::operator+(const Position &add) const {
    requireSameDimensions(add, "Position::operator+");
    Position p(dimensions);
    for (int i = 0; i < dimensions; i++)
        p[i] += add[i];
    return p;
}

// Mutator version of addition
Position& Position::operator+=(const Position &add) {
    requireSameDimensions(add, "Position::operator+=");
    for (int i = 0; i < dimensions; i++)
        operator[](i) += add[i];

    return *this;
}

// Subtracting a position from another subtracts the individual elements.
Position Position::operator-(const Position &subtract) const {
    requireSameDimensions(subtract, "Position::operator-");
    Position p(dimensions);
    for (int i = 0; i < dimensions; i++)
        p[i] -= subtract[i];
    return p;
}

// Mutator version of subtraction.
Position& Position::operator-=(const Position &subtract) {
    requireSameDimensions(subtract, "Position::operator-=");
    for (int i = 0; i < dimensions; i++)
        operator[](i) -= subtract[i];

    return *this;
}

// Unary negation of a position is unary negation of each dimension value.
Position Position::operator-() const noexcept {
    Position p(dimensions);
    for (int i = 0; i < dimensions; i++)
        p[i] = -operator[](i);

    return p;
}

// A position can be scaled by a constant, which scales each dimension value.
Position Position::operator*(const double &scale) const noexcept {
    Position p(dimensions);
    for (int i = 0; i < dimensions; i++)
        p[i] *= scale;

    return p;
}

// Mutator version of scaling.
Position& Position::operator*=(const double &scale) noexcept {
    for (int i = 0; i < dimensions; i++)
        operator[](i) *= scale;

    return *this;
}

// Scales each dimension value by 1/d
Position Position::operator/(const double &inv_scale) const noexcept {
    return operator*(1.0/inv_scale);
}

// Mutator version of division scaling.
Position& Position::operator/=(const double &inv_scale) noexcept {
    return operator*=(1.0/inv_scale);
}

std::ostream& operator<<(std::ostream &os, const Position &p) {
    os << "Position[" << p[0];
    for (int i = 1; i < p.dimensions; i++)
        os << ", " << p[i];
    os << "]";
    return os;
}

}
