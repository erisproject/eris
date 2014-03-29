#include "eris/Position.hpp"
#include <cmath>

namespace eris {

Position::Position(const std::initializer_list<double> &coordinates) : Position(std::vector<double>(coordinates)) {}

Position::Position(std::vector<double> &&coordinates) : pos_(coordinates) {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(const Position &pos) : pos_(pos.pos_) {
    *this = pos;
}

Position::Position(Position &&pos) : pos_(std::move(pos.pos_)) {
}

Position Position::zero(const size_t &dimensions) {
    return Position(std::vector<double>(dimensions, 0.0));
}

Position Position::zero(const Position &pos) {
    return zero(pos.dimensions);
}

double& Position::at(int d) { return pos_.at(d); }

const double& Position::at(int d) const { return pos_.at(d); }

#define POSITIONCPP_ITERATOR_MAP(iterator_type, method) \
std::vector<double>::iterator_type Position::method() { return pos_.method(); } \
std::vector<double>::const_##iterator_type Position::method() const { return pos_.method(); } \
std::vector<double>::const_##iterator_type Position::c##method() const { return pos_.c##method(); }
POSITIONCPP_ITERATOR_MAP(iterator, begin)
POSITIONCPP_ITERATOR_MAP(iterator, end)
POSITIONCPP_ITERATOR_MAP(reverse_iterator, rbegin)
POSITIONCPP_ITERATOR_MAP(reverse_iterator, rend)
#undef POSITIONCPP_ITERATOR_MAP

double Position::distance(const Position &other) const {
    requireSameDimensions(other, "Position::distance");

    if (dimensions == 1)
        return fabs(operator[](0) - other[0]);

    if (dimensions == 2)
        return hypot(operator[](0) - other[0], operator[](1) - other[1]);

    // NB: could protect this against underflow/overflow by adapting the hypot algorithm to 3+
    // dimensions; see http://en.wikipedia.org/wiki/Hypot
    double dsq = 0;
    for (size_t i = 0; i < dimensions; ++i)
        dsq += pow(operator[](i) - other[i], 2);
    return sqrt(dsq);
}

double Position::length() const {
    if (dimensions == 1) return fabs(operator[](0));
    if (dimensions == 2) return hypot(operator[](0), operator[](1));

    // NB: could protect this against underflow/overflow by adapting the hypot algorithm to 3+
    // dimensions; see http://en.wikipedia.org/wiki/Hypot
    double lsq = 0.0;
    for (auto &d : pos_)
        lsq += d*d;
    return sqrt(lsq);
}

Position Position::mean(const Position &other, const double &weight) const {
    requireSameDimensions(other, "Position::mean");

    Position result = zero(dimensions);
    double our_weight = 1.0-weight;

    for (size_t i = 0; i < dimensions; i++)
        result[i] = our_weight*operator[](i) + weight*other[i];

    return result;
}

Position Position::subdimensions(std::initializer_list<double> dims) const {
    Position p = zero(dims.size());
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

    for (size_t i = 0; i < dimensions; i++)
        operator[](i) = new_pos[i];

    return *this;
}

Position::operator bool() const {
    for (size_t i = 0; i < dimensions; i++)
        if (operator[](i) != 0) return true;
    return false;
}

bool Position::operator==(const Position &other) const {
    requireSameDimensions(other, "Position::operator==");

    for (size_t i = 0; i < dimensions; i++)
        if (operator[](i) != other[i]) return false;

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
    for (size_t i = 0; i < dimensions; i++)
        operator[](i) += add[i];

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
    for (size_t i = 0; i < dimensions; i++)
        operator[](i) -= subtract[i];

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
    for (size_t i = 0; i < dimensions; i++)
        operator[](i) *= scale;

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
    for (size_t i = 1; i < p.dimensions; i++)
        os << ", " << p[i];
    os << "]";
    return os;
}

}
