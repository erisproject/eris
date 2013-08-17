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

        /** Constructs a PositionalAgent at location `p` whose movements are not constrained.
         */
        PositionalAgent(const Position &p);

        /** Constructs an unbounded PositionalAgent at the given coordinates. */
        PositionalAgent(const std::initializer_list<double> pos);

        /** Returns the current position. */
        const Position& position() const noexcept;

        /** Returns true if a boundary applies to the position of this agent.
         */
        bool bounded() const noexcept;

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

        /** If true, attempting to move to a position outside the agent's bounding box will instead
         * move to the nearest point on the boundary.  If false (the default) attempting to move
         * outside the bounding box will throw an exception.
         */
        bool move_to_nearest = false;

        /** Moves to the given position.  If the position is outside the bounding box,
         * `move_to_nearest` is checked: if true, the agent moves to boundary point closest to the
         * destination; if false, a boundary_error exception is thrown.
         *
         * \returns true if the move was completed as requested, false if the move was corrected to
         * the nearest boundary point.
         * \throws PositionalAgent::boundary_error if move_to_nearest was false and the destination
         * was outside the boundary.
         * \throws std::length_error if p does not have the same dimensions as the agent's position.
         */
        bool moveTo(Position p);

        /** Moves by the given relative amounts.  `a.moveBy(relative) is simply a shortcut for
         * `a.moveTo(a.position() + relative)`.
         *
         * \throws std::length_error if relative does not have the same dimensions as the agent's position.
         */
        bool moveBy(const Position &relative);

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
};

inline const Position& PositionalAgent::position() const noexcept { return position_; }

} }
