#pragma once
#include <eris/Position.hpp>
#include <eris/Positional.hpp>
#include <cstddef>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace eris {

/** Base class for WrappedPositional<T> that works just like Positional<T>'s base class but adds
 * wrapping to one or more of the position dimensions.
 *
 * You can inherit from this directly, or use the WrappedPositional<T> convenience wrapper.
 *
 * \sa WrappedPositional<T>
 */
class WrappedPositionalBase : public PositionalBase {
    public:
        /// Not default constructible
        WrappedPositionalBase() = delete;

        /** Constructs a WrappedPositionalBase at location `p` who has wrapping boundies in all
         * non-infinite dimensions given by the bounding box defined by the two boundary vertex
         * positions.
         *
         * In one dimension, this is a circle; in two dimensions, a doughnut (or torus).  In a
         * general number of dimensions, this is a hypertorus.
         */
        WrappedPositionalBase(const Position &p, const Position &boundary1, const Position &boundary2);

        /** Constructs a WrappedPositionalBase at location `p` who has wrapping boundaries at `b1`
         * and `b2` in every dimension.
         */
        WrappedPositionalBase(const Position &p, double b1, double b2);

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
        template <class Container, std::enable_if_t<std::is_integral<typename Container::value_type>::value, int> = 0>
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
        explicit WrappedPositionalBase(const Position &p);

        /** Returns the shortest distance vector from this object to the given position.  Unlike
         * PositionalBase, this is more complicated because it needs to also consider vectors that
         * cross boundaries.  The returned vector may, thus, point outside the caller's bounding box
         * when added to the caller's position.  This is intentional, as that is the shortest path
         * to the target position (when incorporating wrapping).
         */
        virtual Position vectorTo(const Position &pos) const override;
        using PositionalBase::vectorTo;

        /** Returns true if the given dimension is effectively wrapped.  To be so, three conditions
         * must be satisfied:
         * - Both boundary points must be finite
         * - The upper and lower bounds must not be equal
         * - The boundary must have been specified as wrapping at construction or by a call to
         *   wrap().
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual bool wrapped(size_t dim) const;

        /** Enables wrapping on dimension `dim`, if possible.  Wrapping is not possible if either
         * boundary in the given dimension is infinite, or if the two boundaries are equal.
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual void wrap(size_t dim);

        /** Enables wrapping on multiple dimensions at once.  Can be called with an initializer_list
         * or any other type of container of integer values.
         *
         * \throws std::out_of_range if `dim` is any of the given dimensions is invalid.
         */
        template <class Container, std::enable_if_t<std::is_integral<typename Container::value_type>::value, int> = 0>
        void wrap(const Container &dimensions) {
            for (auto &dim : dimensions) {
                // The below ("<= and !=") is needed to avoid a warning for "dim < 0" when dim
                // is an unsigned type
                if ((dim <= 0 and dim != 0) or dim >= (decltype(dim)) position_.dimensions)
                    throw std::out_of_range("Invalid dimension passed to WrappedPositionalBase::wrap(const Container&)");
                wrap(dim);
            }
        }

        /** Disables wrapping on dimension `dim`.  Dimension `dim` will start acting as a boundary
         * (as in a traditional Positional<T>) rather than a wrapped dimension.
         *
         * \throws std::out_of_range if `dim` is not a valid dimension.
         */
        virtual void unwrap(size_t dim);

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

        /** Returns true if a non-wrapping boundary applies to the position of this object.
         */
        virtual bool bounded() const override;

        /** Returns true if the object's position is currently on one of the non-wrapping boundaries,
         * false otherwise.
         */
        virtual bool binding() const override;

        /** Returns true if the object's position is currently on the lower boundary in any
         * non-wrapping dimension, false otherwise.
         *
         * Note that it is possible for an object's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        virtual bool bindingLower() const override;

        /** Returns true if the object's position is currently on the upper boundary in any
         * non-wrapping dimension, false otherwise.
         *
         * Note that it is possible for an object's position to be on both a lower bound and upper
         * bound simultaneously.
         */
        virtual bool bindingUpper() const override;

        /** Returns the lowest-coordinates vertex of the bounding box.  Both unbounded
         * and wrapped dimension boundaries (which are really wrapping points, not boundaries) are
         * returned as negative infinity.
         */
        virtual Position lowerBound() const override;
        
        /** Returns the highest-coordinates vertex of the bounding box.  Both unbounded and wrapped
         * dimension boundaries (which are really wrapping points, not boundaries) are returned as
         * positive infinity.
         */
        virtual Position upperBound() const override;

        /** Returns the lowest-coordinates vertex of the bounding box, including wrapping
         * boundaries.  Unlike lowerBound(), this returns the wrapping points for wrapped
         * dimensions.  Negative infinity is still returned for unbounded, unwrapped dimensions.
         */
        virtual Position wrapLowerBound() const;

        /** Returns the highest-coordinates vertex of the bounding box, including wrapping
         * boundaries.  Unlike upperBound(), this returns the wrapping points for wrapped
         * dimensions.  Positive infinity is still returned for unbounded, unwrapped dimensions.
         */
        virtual Position wrapUpperBound() const;

    protected:
        /** Truncates the given Position object but first applies dimension wrapping by passing the
         * Position to wrap.  Returns true if non-wrapping truncation was necessary and allowed,
         * false if no changes were needed, and throws a PositionalBoundaryError exception if
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
        std::vector<bool> wrapped_ = std::vector<bool>(position_.dimensions, false);
};

/** This class works just like Positional but adds wrapping to one or more dimensions.
 *
 * In such a model, an attempt to move beyond the wrapping boundary results in a position at the
 * opposite boundary.  For example, a 1-dimensional WrappedPositional<T> with boundaries
 * at -1 and 1 that moves to 1.2 (or 3.2 or 5.2 or ...) will end up at -0.8.
 *
 * Distances also incorporate the wrapped dimensions; continuing the previous example, the distance
 * between a WrappedPositional<T> at 0.9 and -0.9 is 0.2, not 1.8.  Returned distances are always
 * the shortest distance.
 *
 * To use this class, inherit (or use directly) a WrappedPositional<T> wrapper around your desired T
 * object.
 *
 * WrappedPositional<T> objects are compatible with Positional<T> objects for comparisons (e.g.
 * `a.distance(b)` works as expected).
 *
 * Caveats of this class:
 * - Because WrappedPositional<T> objects may have a position on *either* boundary of a wrapped region,
 *   `a.position() == b.position()` implies `a.distance(b) == 0`, but the converse is not
 *   necessarily true: WrappedPositional<T> objects could be at opposite boundaries which are
 *   distance 0 from each other yet are distinct positions.  As such as you always use
 *   `a.distance(b) == 0` to determined whether two positions are effectively the same.
 * - `a.position().distance(b.position())` should not be used as the underlying `Position` object
 *   knows nothing about boundaries.  Instead use `a.distance(b)`.
 * - The wrapping applied for distance calculations is always that of the *calling* object; in other
 *   words, `a.distance(b) == b.distance(a)` need not be true if `a` and `b` have different wrapping
 *   parameters, or if b isn't a WrappedPositional<T> at all.
 * - `a.moveBy(delta); a.moveBy(-delta)` may result in a being at a different position (aside from
 *   numerical imprecision): if the object starts on a boundary and the first movement wraps, the
 *   object will end up at the opposite boundary.
 *
 * \sa WrappedPositionalBase for the available methods to a WrappedPositional<T> object
 * \sa Positional
 */
template <class T>
class WrappedPositional : public WrappedPositionalBase, public T {
    public:
        /** Constructs a WrappedPositional<T> at location `p` who has wrapping boundies in all
         * non-infinite dimensions given by the bounding box defined by the two boundary vertex
         * positions.
         *
         * In one dimension, this is the circumference of a circle; in two dimensions, a doughnut
         * (or torus).  In a general number of dimensions, this is a hypertorus.
         *
         * \param p the initial Position.  `p` is not required to be inside the bounding box, but
         * will be wrapped into the bounding box on any wrapped dimensions.
         *
         * \param boundary1 a bounding box vertex Position.  This must be of the same dimensions as
         * `p`.
         *
         * \param boundary2 a bounding box vertex Position.  This must be of the same dimesions as
         * `p`.
         *
         * \param T_args any extra arguments are forwarded to the constructor of class `T`
         */
        template<typename... Args, std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
        WrappedPositional(const Position &p, const Position &boundary1, const Position &boundary2,
                Args&&... T_args)
            : WrappedPositionalBase(p, boundary1, boundary2), T(std::forward<Args>(T_args)...)
        {}

        /** Constructs a WrappedPositional<T> at location `p` who has wrapping boundies at `b1` and
         * `b2` in all dimensions.
         *
         * \param p the initial Position.  `p` is not required to be inside the bounding box, but
         * will be wrapped into the bounding box on any wrapped dimensions.
         *
         * \param b1 the value (any integral or floating point type; will be converted to double) at
         * which a boundary applies in each dimension of `p`.
         *
         * \param b2 the value (any integral or floating point type; will be converted to double) at
         * which a boundary applies in each dimension of `p`.
         *
         * \param T_args any extra arguments are forwarded to the constructor of class `T`
         */
        template<typename... Args, typename Numeric1, typename Numeric2,
            std::enable_if_t<
                std::is_arithmetic<Numeric1>::value && std::is_arithmetic<Numeric2>::value &&
                std::is_constructible<T, Args...>::value,
                int> = 0>
        WrappedPositional(const Position &p, Numeric1 b1, Numeric2 b2, Args&&... T_args)
            : WrappedPositionalBase(p, (double) b1, (double) b2), T(std::forward<Args>(T_args)...)
        {}

        /** Constructs a WrappedPositional<T> at location `p` who is bounded by the bounding box
         * defined by the two boundary vertex positions with wrapping on one or more dimensions.
         *
         * \param p the initial Position.  `p` is not required to be inside the bounding box, but
         * will be wrapped into the bounding box on any wrapped dimensions.
         *
         * \param boundary1 a bounding box vertex Position.  This must be of the same dimensions as
         * `p`.
         *
         * \param boundary2 a bounding box vertex Position.  This must be of the same dimesions as
         * `p`.
         *
         * \param dimensions any sort of standard iterable container containing integer values of
         * dimension indexes that should wrap.
         *
         * \param T_args any extra arguments are forwarded to the constructor of class `T`
         *
         * \throws std::length_error if `p`, `boundary1`, and `boundary2` are not of the same dimension.
         * \throws std::out_of_range if any element of `dimensions` is not a valid dimension index.
         */
        template <class Container, typename... Args, std::enable_if_t<
            std::is_integral<typename Container::value_type>::value &&
            std::is_constructible<T, Args...>::value,
            int> = 0>
        WrappedPositional(const Position &p, const Position &boundary1, const Position &boundary2, const Container &dimensions,
                Args&&... T_args)
            : WrappedPositionalBase(p, boundary1, boundary2, dimensions), T(std::forward<Args>(T_args)...)
        {}

        /** Constructs a WrappedPositional<T> at location `p` which is bounded by the bounding box
         * defined by the two boundary vertex positions with wrapping on one or more dimensions.
         *
         * \param p the initial Position.  `p` is not required to be inside the bounding box, but
         * will be wrapped into the bounding box on any wrapped dimensions.
         *
         * \param boundary1 a bounding box vertex Position.  This must be of the same dimensions as
         * `p`.
         *
         * \param boundary2 a bounding box vertex Position.  This must be of the same dimesions as
         * `p`.
         *
         * \param dims a initializer list of dimensions that should wrap.
         *
         * \param T_args any extra arguments are forwarded to the constructor of class `T`
         *
         * \throws std::length_error if `p`, `boundary1`, and `boundary2` are not of the same dimension.
         * \throws std::out_of_range if any element of `dims` is not a valid dimension index.
         */
        template <typename... Args, std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
        WrappedPositional(const Position &p, const Position &boundary1, const Position &boundary2, const std::initializer_list<size_t> &dims,
                Args&&... T_args)
            : WrappedPositionalBase(p, boundary1, boundary2, dims), T(std::forward<Args>(T_args)...)
        {}

        /** Constructs a WrappedPositional<T> at location `p` whose movements are not wrapped or
         * constrained.
         *
         * Any extra arguments are forwarded to T's constructor.
         */
        template <typename... Args, std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
        explicit WrappedPositional(const Position &p, Args&&... T_args)
            : WrappedPositionalBase(p), T(std::forward<Args>(T_args)...)
        {}
};

}
// vim:tw=100
