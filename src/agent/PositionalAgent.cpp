#include <eris/agent/PositionalAgent.hpp>
#include <algorithm>
#include <limits>

namespace eris { namespace agent {

PositionalAgent::PositionalAgent(const Position &p, const Position &boundary1, const Position &boundary2)
    : position_(p), bounded_(true), lower_bound_(p.dimensions), upper_bound_(p.dimensions) {
    if (p.dimensions != boundary1.dimensions or p.dimensions != boundary2.dimensions)
        throw std::length_error("position and boundary points have different dimensions");

    for (int d = 0; d < p.dimensions; d++) {
        lower_bound_[d] = std::min(boundary1[d], boundary2[d]);
        upper_bound_[d] = std::max(boundary1[d], boundary2[d]);
    }
}

PositionalAgent::PositionalAgent(const Position &p)
    : position_(p), bounded_(false), lower_bound_(p.dimensions), upper_bound_(p.dimensions) {

    for (int d = 0; d < p.dimensions; d++) {
        lower_bound_[d] = -std::numeric_limits<double>::infinity();
        upper_bound_[d] = std::numeric_limits<double>::infinity();
    }
}

PositionalAgent::PositionalAgent(const std::initializer_list<double> pos)
    : position_(pos), bounded_(false), lower_bound_(position_.dimensions), upper_bound_(position_.dimensions) {

    for (int d = 0; d < position_.dimensions; d++) {
        lower_bound_[d] = -std::numeric_limits<double>::infinity();
        upper_bound_[d] = std::numeric_limits<double>::infinity();
    }
}

PositionalAgent::PositionalAgent(
        const std::initializer_list<double> pos,
        const std::initializer_list<double> boundary1,
        const std::initializer_list<double> boundary2)
    : PositionalAgent(Position(pos), Position(boundary1), Position(boundary2)) {}

PositionalAgent::PositionalAgent(double p)
    : PositionalAgent(Position({p})) {}
PositionalAgent::PositionalAgent(double p, double b1, double b2)
    : PositionalAgent(Position({p}), Position({b1}), Position({b2})) {}
PositionalAgent::PositionalAgent(double px, double py)
    : PositionalAgent(Position({px, py})) {}
PositionalAgent::PositionalAgent(double px, double py, double b1x, double b1y, double b2x, double b2y)
    : PositionalAgent(Position({px, py}), Position({b1x, b1y}), Position({b2x, b2y})) {}

double PositionalAgent::distance(const PositionalAgent &other) const {
    return position().distance(other.position());
}

bool PositionalAgent::bounded() const noexcept {
    return bounded_;
}

bool PositionalAgent::binding() const noexcept {
    if (not bounded_) return false;

    for (int d = 0; d < position_.dimensions; d++) {
        const double &p = position_[d];
        if (p == lower_bound_[d] or p == upper_bound_[d]) return true;
    }

    return false;
}

bool PositionalAgent::bindingLower() const noexcept {
    if (not bounded_) return false;

    for (int d = 0; d < position_.dimensions; d++) {
        if (position_[d] == lower_bound_[d]) return true;
    }

    return false;
}

bool PositionalAgent::bindingUpper() const noexcept {
    if (not bounded_) return false;

    for (int d = 0; d < position_.dimensions; d++) {
        if (position_[d] == upper_bound_[d]) return true;
    }

    return false;
}

const Position& PositionalAgent::lowerBound() const noexcept {
    return lower_bound_;
}

const Position& PositionalAgent::upperBound() const noexcept {
    return upper_bound_;
}

bool PositionalAgent::moveTo(Position p) {
    if (p.dimensions != position_.dimensions)
        throw std::length_error("position and moveTo coordinates have different dimensions");
    bool corrected = false;
    if (bounded_) {
        corrected = truncate(p, not moveToBoundary());
    }

    position_ = p;

    return not corrected;
}

bool PositionalAgent::moveTo(const double &x) {
    return moveTo(Position({x}));
}

bool PositionalAgent::moveTo(const double &x, const double &y) {
    return moveTo(Position({x, y}));
}

bool PositionalAgent::moveBy(const Position &relative) {
    if (relative.dimensions != position_.dimensions)
        throw std::length_error("position and moveBy coordinates have different dimensions");

    return moveTo(position_ + relative);
}

bool PositionalAgent::moveBy(const double &x) {
    return moveBy(Position({x}));
}

bool PositionalAgent::moveBy(const double &x, const double &y) {
    return moveBy(Position({x, y}));
}

Position PositionalAgent::toBoundary(Position pos) const {
    truncate(pos);
    return pos;
}

bool PositionalAgent::truncate(Position &pos, bool throw_on_truncation) const {
    if (!bounded_) return false;

    bool truncated = false;
    for (int d = 0; d < pos.dimensions; d++) {
        double &x = pos[d];
        if (x < lower_bound_[d]) {
            if (throw_on_truncation) throw boundary_error();
            x = lower_bound_[d];
            truncated = true;
        }
        else if (x > upper_bound_[d]) {
            if (throw_on_truncation) throw boundary_error();
            x = upper_bound_[d];
            truncated = true;
        }
    }

    return truncated;
}

} }
