#include <eris/Positional.hpp>
#include <algorithm>
#include <limits>

namespace eris {

PositionalBase::PositionalBase(const Position &p, const Position &boundary1, const Position &boundary2)
    : position_(p), bounded_(true) {
    if (position_.dimensions != boundary1.dimensions or position_.dimensions != boundary2.dimensions)
        throw std::length_error("position and boundary points have different dimensions");

    for (size_t d = 0; d < position_.dimensions; d++) {
        lower_bound_[d] = std::min(boundary1[d], boundary2[d]);
        upper_bound_[d] = std::max(boundary1[d], boundary2[d]);
    }
}

PositionalBase::PositionalBase(const Position &p)
    : position_(p), bounded_(false),
    lower_bound_(Position::zero(position_.dimensions)),
    upper_bound_(Position::zero(position_.dimensions)) {

    for (size_t d = 0; d < position_.dimensions; d++) {
        lower_bound_[d] = -std::numeric_limits<double>::infinity();
        upper_bound_[d] = std::numeric_limits<double>::infinity();
    }
}

double PositionalBase::distance(const PositionalBase &other) const {
    return position().distance(other.position());
}

bool PositionalBase::bounded() const noexcept {
    return bounded_;
}

bool PositionalBase::binding() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        const double &p = position_[d];
        if (p == lower_bound_[d] or p == upper_bound_[d]) return true;
    }

    return false;
}

bool PositionalBase::bindingLower() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        if (position_[d] == lower_bound_[d]) return true;
    }

    return false;
}

bool PositionalBase::bindingUpper() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        if (position_[d] == upper_bound_[d]) return true;
    }

    return false;
}

Position PositionalBase::lowerBound() const noexcept {
    return lower_bound_;
}

Position PositionalBase::upperBound() const noexcept {
    return upper_bound_;
}

bool PositionalBase::moveTo(Position p) {
    if (p.dimensions != position_.dimensions)
        throw std::length_error("position and moveTo coordinates have different dimensions");
    bool corrected = false;
    if (bounded_) {
        corrected = truncate(p, not moveToBoundary());
    }

    position_ = p;

    return not corrected;
}

bool PositionalBase::moveBy(const Position &relative) {
    if (relative.dimensions != position_.dimensions)
        throw std::length_error("position and moveBy coordinates have different dimensions");

    return moveTo(position_ + relative);
}

Position PositionalBase::toBoundary(Position pos) const {
    truncate(pos);
    return pos;
}

bool PositionalBase::truncate(Position &pos, bool throw_on_truncation) const {
    if (!bounded_) return false;

    bool truncated = false;
    for (size_t d = 0; d < pos.dimensions; d++) {
        double &x = pos[d];
        if (x < lower_bound_[d]) {
            if (throw_on_truncation) throw PositionalBoundaryError();
            x = lower_bound_[d];
            truncated = true;
        }
        else if (x > upper_bound_[d]) {
            if (throw_on_truncation) throw PositionalBoundaryError();
            x = upper_bound_[d];
            truncated = true;
        }
    }

    return truncated;
}

}

