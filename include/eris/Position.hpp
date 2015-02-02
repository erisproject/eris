#pragma once
#include <stdexcept>
#include <vector>
#include <ostream>

namespace eris {

/** Class representing a geometric position of arbitrary dimensions.
 */
class Position final {
    public:
        /** Default constructor; creates a null position.  This isn't a valid Position object, but
         * is permitted to allow Position objects to be used where default construction is required.
         */
        Position() = default;

        /// Initialize from initializer_list
        Position(const std::initializer_list<double> &coordinates);

        /** Creates a new Position object at the given position.  The dimension is determined from
         * the length of the provided container.  At least one value must be present.
         *
         * Examples:
         * Position({3.0, -4.1}) // initializer list
         * Position(v) // v is vector<double> (or any sort of iterable container)
         */
        template <class Container, typename = typename std::enable_if<std::is_arithmetic<typename Container::value_type>::value>::type>
        Position(const Container &coordinates) : pos_(coordinates.begin(), coordinates.end()) {
            if (not dimensions)
                throw std::out_of_range("Cannot initialize a Position with 0 dimensions");
        }

        /** Creates a Position at the given vector, taking over the vector for position storage.
         *
         * The vector must have at least one element (0-dimension Positions are invalid).
         */
        Position(std::vector<double> &&coordinates);

        /** Constructs a copy of the given Position object. */
        Position(const Position &pos);

        /** Moves the given Position's values into this one */
        Position(Position &&pos) = default;

        /** Creates a new Position object of the given number of dimensions (>= 1) with initial
         * position of 0 for all dimensions.
         */
        static Position zero(const size_t dimensions);

        /** Returns a random Position vector of length 1 of the given dimensionality.  The
         * returned point is uniform across the surface of a hypersphere of the given dimensions.
         *
         * To do this uniform hypersphere surface picking, this draw a N(0,1) for each dimension,
         * then normalizes to a unit distance.  See
         * http://mathworld.wolfram.com/HyperspherePointPicking.html
         */
        static Position random(const size_t dimensions);

        /** Returns the (Euclidean) distance between one position and another.  Throws a
         * std::length_error exception if the two objects are of different dimensions.  The value
         * returned will always be non-negative.
         */
        double distance(const Position &other) const;

        /** Returns the (Euclidean) distance between this position and the origin.  The value
         * returned will always be non-negative.
         */
        double length() const;

        /** Returns the weighted-mean of this Position object and another Position object.
         *
         * \param other the other Position
         * \param weight the weight to apply to this object, defaulting to 0.5.  The weight controls
         * how far along a line from this object to `other` the returned point will be.  The default,
         * 0.5, returns the midpoint between this and `other`.  A value of 1 results in the same
         * Position as `other`; a value of 0 returns the same Position as this object.  Values
         * greater than 1 and less than 0 are allowed: they return a Position along the line through
         * `this` and `other`, but outside the chord between them.
         * \throws std::length_error if the two Position objects are of different dimensions.
         */
        Position mean(const Position &other, const double weight = 0.5) const;

        /** Returns a new Position whose dimensions are determined by the given dimension indices
         * (which may be repeated).  Thus the dimensionality of the returned Position can be larger
         * or smaller than the source.  At least one dimension index must be given.
         *
         * For example, Position({12, -4, 15, 100}).subdimensions({3,1,1}) returns a Position({100,-4,-4}).
         *
         * \throws std::out_of_range exception if any of the given dimension indices are larger than
         * the dimensionality of the caller Position.
         */
        Position subdimensions(std::initializer_list<double> dimensions) const;

        /** Accesses the Position's `d`th coordinate, where `d=0` is the first dimension, etc.
         * No bounds checking is performed.
         */
        double& operator[](int d);

        /** Accesses the Position's `d`th coordinate, with `d=0` is the first dimension, with bounds
         * checking.
         *
         * \throws std::out_of_range exception for `d >= dimensions`.
         */
        double& at(int d);

        /// Const access to Position coordinates.  No bounds checking.
        const double& operator[](int d) const;

        /// Const access to Position coordinates, with bounds checking.
        const double& at(int d) const;

        ///@{
        /// Returns an iterator to the beginning of the position values
        std::vector<double>::iterator begin();
        std::vector<double>::const_iterator begin() const;
        std::vector<double>::const_iterator cbegin() const;
        ///@}
        ///@{
        /// Returns an iterator to the end of the position values
        std::vector<double>::iterator end();
        std::vector<double>::const_iterator end() const;
        std::vector<double>::const_iterator cend() const;
        ///@}
        ///@{
        /// Returns an reverse iterator to the beginning of the position values
        std::vector<double>::reverse_iterator rbegin();
        std::vector<double>::const_reverse_iterator rbegin() const;
        std::vector<double>::const_reverse_iterator crbegin() const;
        ///@}
        ///@{
        /// Returns an reverse iterator to the beginning of the position values
        std::vector<double>::reverse_iterator rend();
        std::vector<double>::const_reverse_iterator rend() const;
        std::vector<double>::const_reverse_iterator crend() const;
        ///@}

        /** Boolean operator: returns true if the Position has at least one non-zero coordinates.
         * Logically equivalent to `pos == Position::zero(pos.dimensions)`, but more efficient.
         */
        explicit operator bool() const;

        /// Equality; true iff all coordinates have the same value.
        bool operator==(const Position &other) const;

        /// Inequality; true iff equality is false.
        bool operator!=(const Position &other) const;

        /** Sets the position to that of the provided position.  The dimensions of the current and
         * new position must be identical.
         */
        Position& operator=(const Position &new_pos);

        /** Sets the position to the values in the provided vector.  The dimension of the current
         * position must match the length of the vector.
         */
        Position& operator=(const std::vector<double> &new_coordinates);

        /** Replaces the internal std::vector with the given one.  The dimensions of the current
         * position must match the length of the replacement vector.
         */
        Position& operator=(std::vector<double> &&new_coordinates);

        /** Move assignment operator. */
        Position& operator=(Position &&move_pos) = default;

        /// Adding two positions together adds the underlying position values together.
        Position operator+(const Position &add) const;

        /// Mutator version of addition
        Position& operator+=(const Position &add);

        /// Subtracting a position from another subtracts the individual elements.
        Position operator-(const Position &subtract) const;

        /// Mutator version of subtraction.
        Position& operator-=(const Position &subtract);

        /// Unary negation of a position is unary negation of each dimension value.
        Position operator-() const noexcept;

        /// A position can be scaled by a constant, which scales each dimension value.
        Position operator*(const double scale) const noexcept;

        /// Scales a position by a constant, with the constant on the left-hand-side.
        friend Position operator*(const double scale, const Position &p);

        /// Mutator version of scaling.
        Position& operator*=(const double scale) noexcept;

        /// Scales each dimension value by 1/d
        Position operator/(const double inv_scale) const noexcept;

        /// Mutator version of division scaling.
        Position& operator/=(const double inv_scale) noexcept;

        /** Overloaded so that a Position can be sent to an output stream, resulting in output such
         * as `Position[0.25, 0, -44.3272]`.
         */
        friend std::ostream& operator<<(std::ostream &os, const Position &p);

    private:
        /// The vector
        std::vector<double> pos_;

        /// Throws an exception if the current object and `other` have different dimensions.
        void requireSameDimensions(const Position &other, const std::string &method) const;

        /// Throws an exception if the current object doesn't match the given number of dimensions.
        void requireSameDimensions(size_t dimensions, const std::string &method) const;

    public:
        /** Accesses the number of dimensions of this Position object.  This will always be at least
         * 1, and cannot be changed.
         */
        // (Declared here because it depends on pos_, which gets initialized during construction)
        const size_t dimensions = pos_.size();
};

inline void Position::requireSameDimensions(size_t dim, const std::string &method) const {
    if (dimensions != dim)
        throw std::length_error(method + "() called with objects of differing dimensions");
}
inline void Position::requireSameDimensions(const Position &other, const std::string &method) const {
    requireSameDimensions(other.dimensions, method);
}

inline double& Position::operator[](int d) { return pos_[d]; }
inline const double& Position::operator[](int d) const { return pos_[d]; }

}
