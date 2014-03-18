#include <eris/WrappedPositional.hpp>
#include <algorithm>
#include <limits>
#include <cmath>

namespace eris {

WrappedPositionalBase::WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2)
    : PositionalBase(p, boundary1, boundary2) {
    for (size_t d = 0; d < p.dimensions; d++)
        wrap(d);
    wrap(position_);
}

WrappedPositionalBase::WrappedPositionalBase(const Position &p) : PositionalBase(p) {}

WrappedPositionalBase::WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2, const std::initializer_list<size_t> &dims)
    : PositionalBase(p, boundary1, boundary2) {
    wrap(dims);
    wrap(position_);
}

bool WrappedPositionalBase::wrapped(const size_t &dim) const {
    return wrapped_.count(dim) > 0;
}

void WrappedPositionalBase::wrap(const size_t &dim) {
    if (dim >= position_.dimensions)
        throw std::out_of_range("Invalid dimension passed to WrappedPositionalBase::wrap(const size_t&)");

    if (wrapped_.count(dim) > 0) return; // Already wrapping

    if (not std::isfinite(upper_bound_[dim]) or not std::isfinite(lower_bound_[dim]))
        return; // Can't wrap a non-finite dimension

    if (lower_bound_[dim] >= upper_bound_[dim])
        return; // Can't wrap a zero-width dimension

    wrapped_.insert(dim);
}

void WrappedPositionalBase::unwrap(const size_t &dim) {
    if (dim >= position_.dimensions)
        throw std::out_of_range("Invalid dimension passed to WrappedPositionalBase::wrap(const size_t&)");

    wrapped_.erase(dim);
}

Position WrappedPositionalBase::wrap(const Position &pos) const {
    Position p(pos);
    wrap(p);
    return p;
}

void WrappedPositionalBase::wrap(Position &pos) const {
    if (pos.dimensions != position_.dimensions)
        throw std::length_error("position() and given pos have different dimensions in WrappedPositionalBase::wrap(pos)");
    for (auto &dim : wrapped_) {
        const double &left = lower_bound_[dim], &right = upper_bound_[dim];
        double &x = pos[dim];
        if (x < left) {
            // e.g. [l,r] = [11,14], x = 3.1.  That's 11-3.1=7.9 to the left of the left boundary.  Take the
            // remainder of 7.9/(14-11): which is 1.9: that's the distance in from the right boundary: i.e. position at
            // 14-1.9=12.1.
            double d = fmod(left - x, right - left);

            // If we end up *exactly* on the wrapping boundary, position on the lower boundary (since
            // our point is off the lower side)
            if (d == 0) x = left;
            else x = right - d;
        }
        else if (x > right) {
            // e.g. [l,r] = [11,14], x = 21.4.  That's 21.4-14=7.4 right of the right boundary.  Take the
            // remainder of 7.4/(14-11), which is 1.4: that's the distance in from the left
            // boundary: i.e. position at 11+1.4=12.4
            double d = fmod(x - right, right - left);

            // If we end up *exactly* on the wrapping boundary, position on the upper boundary
            // (since our point is off the upper side)
            if (d == 0) x = right;
            else x = left + d;
        }
        // else we're already inside the region bounds
    }
}

double WrappedPositionalBase::distance(const PositionalBase &other) const {
    // Build the nearest virtual position of the other agent, starting at their current position
    // (wrapped by *our* wrapping parameters).
    Position v(wrap(other.position()));

    for (auto &dim : wrapped_) {
        const double &me = position_[dim];
        double &them = v[dim];

        // Don't do anything if already at the same position:
        if (me == them) continue;

        // Figure out the potential virtual coordinate: if the other guy is right of me, the
        // alternative is his position going left; if the other guy is left, it's his position going
        // right.
        // 
        // So, if the positions are 1 and 3 with wrapped boundaries at [0, 5], the virtual positions
        // from 1's perspective are -2 (across the left boundary) and 3 (not across the boundary).  For
        // 3's perspective, the two positions are 1 (not across the boundary) and 6 (across the
        // boundary).
        double width = upper_bound_[dim] - lower_bound_[dim];
        double v_alt = me < them
            ? them - width // The other guy is right, so the v_alt is in the left virtual space
            : them + width; // The other guy is left, so v_alt is in the right virtual space

        // If the distance to the alternate is shorter, use it
        if (fabs(me - v_alt) < fabs(me - them))
            them = v_alt;
    }

    return position().distance(v);
}

bool WrappedPositionalBase::bounded() const noexcept {
    for (size_t d = 0; d < position_.dimensions; d++) {
        if (not wrapped_.count(d) and (std::isfinite(lower_bound_[d]) or std::isfinite(upper_bound_[d])))
            return true; // Not wrapped and has a non-finite bound
    }
    return false;
}

bool WrappedPositionalBase::binding() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        const double &p = position_[d];
        if (not wrapped_.count(d) and
                (p == lower_bound_[d] or p == upper_bound_[d]))
            // Not a wrapping dimension, and we're sitting on one of the boundaries
            return true;
    }
    return false;
}

bool WrappedPositionalBase::bindingLower() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        const double &p = position_[d];
        if (not wrapped_.count(d) and p == lower_bound_[d])
            // Not a wrapping dimension, and we're sitting on the lower bound
            return true;
    }
    return false;
}

bool WrappedPositionalBase::bindingUpper() const noexcept {
    if (not bounded_) return false;

    for (size_t d = 0; d < position_.dimensions; d++) {
        const double &p = position_[d];
        if (not wrapped_.count(d) and p == upper_bound_[d])
            // Not a wrapping dimension, and we're sitting on the upper bound
            return true;
    }
    return false;
}

Position WrappedPositionalBase::lowerBound() const noexcept {
    Position lower(lower_bound_);

    for (size_t d = 0; d < position_.dimensions; d++)
        if (wrapped_.count(d)) lower[d] = -std::numeric_limits<double>::infinity();

    return lower;
}

Position WrappedPositionalBase::upperBound() const noexcept {
    Position upper(upper_bound_);

    for (size_t d = 0; d < position_.dimensions; d++)
        if (wrapped_.count(d)) upper[d] = std::numeric_limits<double>::infinity();

    return upper;
}

Position WrappedPositionalBase::wrapLowerBound() const noexcept {
    return lower_bound_;
}

Position WrappedPositionalBase::wrapUpperBound() const noexcept {
    return upper_bound_;
}

bool WrappedPositionalBase::truncate(Position &pos, bool throw_on_truncation) const {
    if (!bounded_) return false;
    wrap(pos);
    return PositionalBase::truncate(pos, throw_on_truncation);
}

}

// vim:tw=100
