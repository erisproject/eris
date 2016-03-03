#include <eris/Position.hpp>
#include <eris/random/rng.hpp>
#include <boost/random/uniform_on_sphere.hpp>
#include <boost/random/bernoulli_distribution.hpp>
#include <boost/version.hpp>
#include <cmath>

namespace eris {

Position::Position(const std::initializer_list<double> &coordinates) : Position(std::vector<double>(coordinates)) {}

Position::Position(const std::vector<double> &coordinates) : pos_{coordinates} {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position::Position(std::vector<double> &&coordinates) : pos_{std::move(coordinates)} {
    if (not dimensions)
        throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
}

Position Position::zero(const size_t dimensions) {
    return Position(std::vector<double>(dimensions, 0.0));
}

Position Position::random(const size_t dimensions) {
// Work around boost bug #11454, introduced in 1.56.0, fixed after 1.60.0
#if BOOST_VERSION >= 105600 && BOOST_VERSION <= 106000
    if (dimensions == 1) return Position(std::vector<double>(1, boost::random::bernoulli_distribution<>()(random::rng()) ? 1.0 : -1.0));
#endif
    return Position(boost::random::uniform_on_sphere<double, std::vector<double>>(dimensions)(random::rng()));
}

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
    return (*this - other).length();
}

double Position::length() const {
    if (dimensions == 1) return std::fabs(pos_[0]);
    if (dimensions == 2) return std::hypot(pos_[0], pos_[1]);

    // NB: could protect this against underflow/overflow by adapting the hypot algorithm to 3+
    // dimensions; see http://en.wikipedia.org/wiki/Hypot
    double lsq = 0.0;
    for (auto &d : pos_)
        lsq += d*d;
    return sqrt(lsq);
}

Position Position::mean(const Position &other, const double weight) const {
    requireSameDimensions(other, "Position::mean");

    Position result = zero(dimensions);
    const double our_weight = 1.0-weight;

    for (size_t i = 0; i < dimensions; i++)
        result[i] = our_weight*pos_[i] + weight*other.pos_[i];

    return result;
}

Position Position::subdimensions(const std::initializer_list<size_t> &dims) const {
    return subdimensions<std::initializer_list<size_t>>(dims);
}

Position& Position::operator=(const Position &new_pos) {
    if (dimensions == 0) {
        // We have a default-constructed object, so allow 0
        const_cast<size_t&>(dimensions) = new_pos.dimensions;
        pos_.resize(dimensions);
    }
    requireSameDimensions(new_pos, "Position::operator=");

    for (size_t i = 0; i < dimensions; i++)
        pos_[i] = new_pos.pos_[i];

    return *this;
}

Position& Position::operator=(const std::vector<double> &new_coordinates) {
    requireSameDimensions(new_coordinates.size(), "Position::operator=");
    pos_ = new_coordinates;
    return *this;
}

Position& Position::operator=(std::vector<double> &&new_coordinates) {
    requireSameDimensions(new_coordinates.size(), "Position::operator=");
    pos_ = std::move(new_coordinates);
    return *this;
}

Position::operator bool() const {
    for (size_t i = 0; i < dimensions; i++)
        if (pos_[i] != 0) return true;
    return false;
}

bool Position::operator==(const Position &other) const {
    requireSameDimensions(other, "Position::operator==");

    for (size_t i = 0; i < dimensions; i++)
        if (pos_[i] != other.pos_[i]) return false;

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
        pos_[i] += add.pos_[i];

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
        pos_[i] -= subtract.pos_[i];

    return *this;
}

// Unary negation of a position is unary negation of each dimension value.
Position Position::operator-() const noexcept {
    return *this * -1.0;
}

// A position can be scaled by a constant, which scales each dimension value.
Position Position::operator*(const double scale) const noexcept {
    return Position(*this) *= scale;
}

Position operator*(const double scale, const Position &p) {
    return p * scale;
}

// Mutator version of scaling.
Position& Position::operator*=(const double scale) noexcept {
    for (size_t i = 0; i < dimensions; i++)
        pos_[i] *= scale;

    return *this;
}

// Scales each dimension value by 1/d
Position Position::operator/(const double inv_scale) const noexcept {
    return Position(*this) *= (1.0/inv_scale);
}

// Mutator version of division scaling.
Position& Position::operator/=(const double inv_scale) noexcept {
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
