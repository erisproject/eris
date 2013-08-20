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

bool PositionalAgent::bounded() const noexcept {
    return bounded_;
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
        for (int d = 0; d < p.dimensions; d++) {
            double &x = p[d];
            if (x < lower_bound_[d]) {
                x = lower_bound_[d];
                corrected = true;
            }
            else if (x > upper_bound_[d]) {
                x = upper_bound_[d];
                corrected = true;
            }

            if (corrected and not move_to_nearest) throw boundary_error();
        }
    }

    position_ = p;

    return not corrected;
}

bool PositionalAgent::moveBy(const Position &relative) {
    if (relative.dimensions != position_.dimensions)
        throw std::length_error("position and moveBy coordinates have different dimensions");

    return moveTo(position_ + relative);
}

} }
