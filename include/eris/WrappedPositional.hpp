#pragma once
#include <eris/Positional.hpp>
#include <set>

namespace eris {

/** Base class for Positional<T> that adds wrapping to one or more of the position dimensions.  In
 * such a model, an attempt to move beyond the wrapping boundary results in a position at the
 * opposite boundary.  For example, a 1-dimensional Positional<T, WrappedPositional> with boundaries
 * at -1 and 1 that moves to 1.2 (or 3.2 or 5.2 or ...) will end up at -0.8.
 *
 * Distances also incorporate the wrapped dimensions; continuing the previous example, the distance
 * between a WrappedPositional<T> at 0.9 and -0.9 is 0.2, not 1.8.  Returned distances are always
 * the shortest distance.
 *
 * To use this class, inherit (or use directly) a Positional<T, WrappedPositionalBase> wrapper
 * around your desired T object, or the equivalent typedef: WrappedPositional<T>.
 *
 * Caveats of this class:
 * - Because wrapped Positional<T> objects may have a position on *either* boundary of a wrapped region,
 *   `a.position() == b.position()` implies `a.distance(b) == 0`, but the converse is not
 *   necessarily true: Positional<T> objects could be at opposite boundaries which are distance 0
 *   from each other yet are distinct positions.  As such as you always use `a.distance(b) == 0` to
 *   determined whether two positions are effectively the same.
 * - `a.position().distance(b.position())` should not be used as the underlying `Position` object
 *   knows nothing about boundaries.  Instead use `a.distance(b)`.
 * - `a.moveBy(delta); a.moveBy(-delta)` may result in a being at a different position (aside from
 *   numerical imprecision): if the agent starts on a boundary and the first movement wraps, the
 *   agent will end up at the opposite boundary.
 * - The wrapping applied for distance calculations is always that of the *calling* object; in other
 *   words, `a.distance(b) == b.distance(a)` need not be true if `a` and `b` have different wrapping
 *   parameters.
 */
class WrappedPositionalBase : public PositionalBase {
    public:
        WrappedPositionalBase() = delete;

        /** Constructs a WrappedPositionalBase at location `p` who has wrapping boundies in all
         * non-infinite dimensions given by the bounding box defined by the two boundary vertex
         * positions.
         *
         * In one dimension, this is a circle; in two dimensions, a doughnut (or torus).  In a
         * general number of dimensions, this is a hypertorus.
         */
        WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2);

        /** Constructs a WrappedPositionalBase at location `p` who is bounded by the bounding box defined
         * by the two boundary vertex positions with wrapping on one or more dimensions.
         *
         * `p`, `boundary`, and `boundary2` must be of the same dimensions.  `p` is not required to
         * be inside the bounding box, but will be wrapped into the bounding box on any wrapped
         * dimensions.
         *
         * `dimensions` is a container of `size_t` elements specifying the dimensions on which
         * wrapping applies.  Not that only dimensions for which both bounds are finite and not
         * equal will wrap.
         *
         * \throws std::length_error if p, boundary1, and boundary2 are not of the same dimension.
         * \throws std::out_of_range if any element of `dimensions` is not a valid dimension number.
         */
        template <class Container, typename = typename std::enable_if<std::is_integral<typename Container::value_type>::value>::type>
        WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2, const Container &dimensions)
            : PositionalBase(p, boundary1, boundary2)
        {
            wrap(dimensions);
            wrap(position_);
        }

        /** Same as above, but explicitly with a size_t initializer_list 
         */
        WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2, const std::initializer_list<size_t> &dims);

        /** Constructs a WrappedPositionalBase at location `p` whose movements are not constrained (and
         * thus do not wrap).
         */
        WrappedPositionalBase(const Position &p);

        /** Returns to distance from this agent's position to the passed-in agent's position.  This
         * overrides PositionalAgent::distance to incorporate dimension wrapping.  The returned
         * dimension is the shortest path between the two agents.
         *
         * Note that only the caller's wrapping is applied to both agents' position: if `other` has
         * different wrapping boundaries, `a.distance(b)` and `b.distance(a)` are not necessarily
         * equal.
         */
        virtual double distance(const PositionalBase &other) const override;

        /** Returns true if the given dimension is effectively wrapped.  To be so, three conditions
         * must be satisfied:
         * - Both boundary points must be finite
         * - The upper and lower bounds must not be equal
         * - The boundary must have been specified as wrapping at construction or by a call to
         *   wrap().
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual bool wrapped(const size_t &dim) const;

        /** Enables wrapping on dimension `dim`, if possible.  Wrapping is not possible if either
         * boundary in the given dimension is infinite, or if the two boundaries are equal.
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual void wrap(const size_t &dim);

        /** Enables wrapping on multiple dimensions at once.  Can be called with an initializer_list
         * or any other type of container of integer values.
         *
         * \throws std::out_of_range if `dim` is any of the given dimensions is invalid.
         */
        template <class Container, typename = typename std::enable_if<std::is_integral<typename Container::value_type>::value>::type>
        void wrap(const Container &dimensions) {
            for (auto &dim : dimensions) {
                // The below ("<= and not ==") is needed to avoid a warning for "dim < 0" when dim
                // is an unsigned type
                if ((dim <= 0 and not dim == 0) or dim >= (decltype(dim)) position_.dimensions)
                    throw std::out_of_range("Invalid dimension passed to WrappedPositionalBase::wrap(const Container&)");
                wrap(dim);
            }
        }

        /** Disables wrapping on dimension `dim`.  Dimension `dim` will start acting as a boundary
         * (as in a traditional PositionalAgent) rather than a wrapped dimension.
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual void unwrap(const size_t &dim);

        /** Applies the currently active wrapping to the given Position, returning a new Position
         * with all wrapped dimensions wrapped into a value between the two dimension boundaries.
         *
         * Values outside boundaries in unwrapped dimensions are changed.
         *
         * Example:
         *     WrappedPositionalBase cpa({1}, {-1}, {5});
         *     cpa.wrap({19}); // == Position({1})
         *
         * Note: not virtual as this simply calls the other wrap method.
         */
        Position wrap(const Position &pos) const;

        /** Just like wrap(), above, but directly alters the given Position. */
        virtual void wrap(Position &pos) const;

        /** Returns true if a non-wrapping boundary applies to the position of this agent.
         */
        virtual bool bounded() const noexcept override;

        /** Returns true if the agent's position is currently on one of the non-wrapping boundaries,
         * false otherwise.
         */
        virtual bool binding() const noexcept override;

        /** Returns true if the agent's position is currently on the lower boundary in any
         * non-wrapping dimension, false otherwise.
         *
         * Note that it is possible for an agent's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        virtual bool bindingLower() const noexcept override;

        /** Returns true if the agent's position is currently on the upper boundary in any
         * non-wrapping dimension, false otherwise.
         *
         * Note that it is possible for an agent's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        virtual bool bindingUpper() const noexcept override;

        /** Returns the lowest-coordinates vertex of the bounding box.  Both unbounded
         * and wrapped dimension boundaries (which are really wrapping points, not boundaries) are
         * returned as negative infinity.
         */
        virtual Position lowerBound() const noexcept override;
        
        /** Returns the highest-coordinates vertex of the bounding box.  Both unbounded and wrapped
         * dimension boundaries (which are really wrapping points, not boundaries) are returned as
         * positive infinity.
         */
        virtual Position upperBound() const noexcept override;

        /** Returns the lowest-coordinates vertex of the bounding box, including wrapping
         * boundaries.  Unlike lowerBound(), this returns the wrapping points for wrapped
         * dimensions.  Negative infinity is still returned for unbounded, unwrapped dimensions.
         */
        virtual Position wrapLowerBound() const noexcept;

        /** Returns the highest-coordinates vertex of the bounding box, including wrapping
         * boundaries.  Unlike upperBound(), this returns the wrapping points for wrapped
         * dimensions.  Positive infinity is still returned for unbounded, unwrapped dimensions.
         */
        virtual Position wrapUpperBound() const noexcept;

    protected:
        /** Truncates the given Position object but first applies dimension wrapping by passing the
         * Position to wrap.  Returns true if non-wrapping truncation was necessary and allowed,
         * false if no changes were needed, and throws a boundary_error exception if
         * throw_on_truncation is true if changes are needed but not allowed.
         *
         * (Since wrapping can be turned on only for particular dimensions, other dimensions might
         * need truncation.)
         *
         * This simply wraps the given point, then calls PositionalBase::truncate to perform the
         * truncation.
         */
        virtual bool truncate(Position &pos, bool throw_on_truncation = false) const override;

    private:
        // The set of wrapped dimensions
        std::set<size_t> wrapped_;
};

/// WrappedPositional<T> is an alias for Positional<T, WrappedPositionalBase> for convenience.
template<class T> using WrappedPositional = Positional<T, WrappedPositionalBase>;

}

// vim:tw=100
