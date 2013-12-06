#pragma once
#include <eris/Agent.hpp>
#include <eris/Position.hpp>
#include <stdexcept>

namespace eris { namespace agent {

/** Agent subclass that adds a position and optional bounding box to an agent.  This is intended for
 * use in models where spatial location matters, such as political economy voting models. */
class PositionalAgent : public Agent {
    public:
        PositionalAgent() = delete;

        /** Constructs a PositionalAgent at location `p` who is bounded by the bounding box defined
         * by the two boundary vertex positions.
         *
         * p, boundary1, and boundary2 must be of the same dimensions.  p is not required to be
         * inside the bounding box (though subsequent moves will be).
         *
         * \throws std::length_error if p, boundary1, and boundary2 are not of the same dimension.
         */
        PositionalAgent(const Position &p, const Position &boundary1, const Position &boundary2);

        /** Rvalue reference version of three-position constructor.
         */
        PositionalAgent(const Position &&p, const Position &&boundary1, const Position &&boundary2);

        /** Constructs a PositionalAgent at location `p` whose movements are not constrained.
         */
        PositionalAgent(const Position &p);

        /** One-dimensional constructor takes a single position, no bounds. */
        PositionalAgent(double p);
        /** One-dimensional constructor takes a single position and two bounds. */
        PositionalAgent(double p, double b1, double b2);
        /** Two-dimensional constructor takes a pair of 2d positions, no bounds. */
        PositionalAgent(double px, double py);
        /** Two-dimensional constructor takes 6 double values: the first two are the 2d positions,
         * the second two are the first bounding box vertex, the last two are the second bounding
         * box vertex. */
        PositionalAgent(double px, double py, double b1x, double b1y, double b2x, double b2y);

        /** Returns the current position. */
        const Position& position() const noexcept;

        /** Returns to distance from this agent's position to the passed-in agent's position.  This
         * is simply an alias for obj.position().distance(target.position()), though subclasses
         * could override.
         */
        virtual double distance(const PositionalAgent &other) const;

        /** Returns true if a boundary applies to the position of this agent.
         */
        bool bounded() const noexcept;

        /** Returns true if the agent's position is currently on one of the boundaries, false
         * otherwise. */
        bool binding() const noexcept;

        /** Returns true if the agent's position is currently on the lower boundary in any
         * dimension, false otherwise.
         *
         * Note that it is possible for an agent's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        bool bindingLower() const noexcept;

        /** Returns true if the agent's position is currently on the upper boundary in any
         * dimension, false otherwise.
         *
         * Note that it is possible for an agent's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        bool bindingUpper() const noexcept;

        /** Returns the lowest-coordinates vertex of the bounding box.  For example, if bounding box
         * vertices of (1,-1,5) and (0,2,3) are given, this method returns the position (0,-1,3).
         * If the agent is not bounded, this returns negative infinity for each dimension.
         */
        const Position& lowerBound() const noexcept;
        
        /** Returns the highest-coordinates vertex of the bounding box.  For example, if bounding box
         * vertices of (1,-1,5) and (0,2,3) are given, this method returns the position (1,2,5).  If
         * the agent is not bounded, this returns infinity for each dimension.
         */
        const Position& upperBound() const noexcept;

        /** Returns true if attempting to move to a position outside the agent's bounding box should
         * instead move to the nearest point on the boundary.  The default always returns false;
         * subclasses should override if desired.
         */
        virtual bool moveToBoundary() const noexcept { return false; }

        /** Moves to the given position.  If the position is outside the bounding box,
         * `moveToBoundary()` is checked: if true, the agent moves to boundary point closest to the
         * destination; if false, a boundary_error exception is thrown.
         *
         * Only this method is virtual as all other moveTo() and moveBy() methods simply call this
         * method.
         *
         * \returns true if the move was completed as requested, false if the move was corrected to
         * the nearest boundary point.
         * \throws PositionalAgent::boundary_error if moveToBoundary() was false and the destination
         * was outside the boundary.
         * \throws std::length_error if p does not have the same dimensions as the agent's position.
         */
        virtual bool moveTo(Position p);

        /// Just like moveTo(Position), but takes a single coordinate for a 1-d Position object
        bool moveTo(const double &x);

        /// Just like moveTo(Position), but takes a pair of coordinates for a 2-d Position object
        bool moveTo(const double &x, const double &y);

        /** Moves by the given relative amounts.  `a.moveBy(relative) is simply a shortcut for
         * `a.moveTo(a.position() + relative)`.
         *
         * \returns true if the move was completed as requested, false if the move was corrected to
         * the nearest boundary point.
         * \throws PositionalAgent::boundary_error if moveToBoundary() was false and the destination
         * was outside the boundary.
         * \throws std::length_error if `relative` does not have the same dimensions as the agent's
         * position.
         */
        bool moveBy(const Position &relative);

        /// Just like moveBy(Position), but takes a single coordinate for a 1-d Position object
        bool moveBy(const double &dx);

        /// Just like moveBy(Position), but takes a pair of coordinates for a 2-d Position object
        bool moveBy(const double &dx, const double &dy);

        /** Returns a Position that is as close as the given Position as possible, but within the
         * agent's boundary.  If the agent is unbounded, or the given point is not outside the
         * boundary, this is the same coordinate as given; if the point is outside the boundary, the
         * returned point will be on the boundary.
         */
        Position toBoundary(Position pos) const;

        /// Exception class thrown if attempting to move to a point outside the bounding box.
        class boundary_error : public std::range_error {
            public:
                boundary_error() : std::range_error("Cannot move outside bounding box") {}
        };

    protected:
        /// The current position.
        Position position_;
        /// True if a bounding box applies, false otherwise.
        bool bounded_;
        /// The lower vertex of the bounding box, defined by the lesser value in each dimension.
        Position lower_bound_;
        /// The upper vertex of the bounding box, defined by the greater value in each dimension.
        Position upper_bound_;

        /** Truncates the given Position object, updating (or throwing an exception) if required.
         * Returns true if truncation was necessary and allowed, false if no changes were needed,
         * and throws a boundary_error exception if throw_on_truncation is true if changes are
         * needed but not allowed.
         */
        virtual bool truncate(Position &pos, bool throw_on_truncation = false) const;
};

inline const Position& PositionalAgent::position() const noexcept { return position_; }

} }

// vim:tw=100
