#pragma once
#include <memory>

namespace eris {

class Matrix; // Forward declaration

namespace matrix {

/** This class is the companion of the Matrix class and serves as a base class for any
 * matrix-implementing class.  Classes implementing this base class may safely assume that arguments
 * passed to methods (which are formally MatrixImpl objects) are actually instances of the derived
 * class, and thus may safely use `static_cast<Derived&>` to cast the passed MatrixImpl object
 * into the Derived object without requiring the runtime overhead of using `dynamic_cast`.
 *
 * Classes using this base class for matrix operations must take care to ensure that different
 * Matrix objects are never combined (so that the above `static_cast` is safe).  In other words,
 * using two Matrix objects with two different implementations is not generally allowed (unless
 * explicitly supported by the two implementing classes).
 *
 * The methods of this class should never be evoked externally except by the Matrix class.  As such,
 * all methods are declared protected, with Matrix itself being declared a friend.
 */
class MatrixImpl {
    public:
        /// Ref is an alias for std::shared_ptr<MatrixImpl> for convenience.
        using Ref = std::shared_ptr<MatrixImpl>;

        /// Default virtual destructor
        virtual ~MatrixImpl() = default;

    protected:
        friend class eris::Matrix;
        /** Returns false.  Subclasses other than NullImpl should not override this: this is used to
         * detect when an implementation is actually the NullImpl.
         */
        virtual bool null() const { return false; }
        /** Clone creates a duplicate of the matrix, with the same size and coefficients.  The
         * default implementation calls create() then invokes the = operator to set the created
         * matrix equal to the current one; subclasses should override if this can be done more
         * efficiently.
         */
        virtual Ref clone() const;
        /** Returns the number of rows of this matrix. */
        virtual unsigned int rows() const = 0;
        /** Returns the number of columns of this matrix. */
        virtual unsigned int cols() const = 0;
        /** Read-only access to coefficients of this matrix. */
        virtual const double& get(unsigned int r, unsigned int c) const = 0;
        /** Mutating access to coefficients of this matrix. */
        virtual void set(unsigned int r, unsigned int c, double d) = 0;
        /** Creates a new matrix of the given size using the same implementation as the current
         * object.  The initial values of the matrix do not need to be initialized to any particular
         * value.
         */
        virtual Ref create(unsigned int rows, unsigned int cols) const = 0;
        /** Creates a new matrix of the given size using the same implementation as the current
         * object.  The initial values of the matrix must all be set to the given value.
         */
        virtual Ref create(unsigned int rows, unsigned int cols, double initial) const = 0;
        /** Resizes the matrix to the given size.  This will only be called on a matrix that is
         * actually a matrix, not a matrix block view.
         *
         * Implementing classes are not required to retain consistency of blocks derived from this
         * matrix.
         */
        virtual void resize(unsigned int rows, unsigned int cols) = 0;
        /** Creates a new square identity matrix of the given size using the same implementation as
         * the current object.
         */
        virtual Ref identity(unsigned int size) const = 0;

        /** Returns a block view of the matrix.  This is not a copy of the matrix, but an actual
         * reference to a block of the matrix that can be modified to adjust the original matrix and
         * vice versa.
         *
         * \param rowOffset the row index of this matrix corresponding to row 0 of the returned
         * matrix view.
         * \param colOffset the column index of this matrix corresponding to row 0 of the returned
         * matrix view.
         * \param nRows the number of rows the matrix view should have.
         * \param nCols the number of columns the matrix view should have.
         *
         * Note that this method, when called from Matrix, has already checked that the parameters
         * conform with the size of the matrix.
         */
        virtual Ref block(unsigned int rowOffset, unsigned int colOffset, unsigned int nRows, unsigned int nCols) const = 0;

        /// Assigns the values of matrix `b` to this matrix.
        virtual void operator=(const MatrixImpl &b) = 0;

        /// Adds a matrix `b` to this matrix.
        virtual void operator+=(const MatrixImpl &b) = 0;

        /// Subtracts a matrix `b` from this matrix.
        virtual void operator-=(const MatrixImpl &b) = 0;

        /** Adds two matrices together.  The default implementation clones and then invokes the
         * mutator version of this operator on the cloned object, which is then returned.
         * Implementing classes may wish to override this when a more efficient implementation is
         * available.
         */
        virtual Ref operator+(const MatrixImpl &b) const;

        /** Subtracts `b` from this matrix, returning the result in a new matrix.  The default
         * implementation clones and then invokes the mutator version of this operator on the cloned
         * object, which is then returned.  Implementing classes may wish to override this when a
         * more efficient implementation is available.
         */
        virtual Ref operator-(const MatrixImpl &b) const;

        /// Multiplies this matrix by the matrix `b`, returning the result in a new matrix.
        virtual Ref operator*(const MatrixImpl &b) const = 0;

        /// Scales a matrix by the given scalar value.
        virtual void operator*=(double d) = 0;

        /** Scales a matrix by the given scalar value, returning the result in a new matrix.  The
         * default implementation clones the object then invokes the mutator version of this
         * operator.
         */
        virtual Ref operator*(double d) const;

        /// Returns the transpose of this matrix.
        virtual Ref transpose() const = 0;

        /** Calculates the rank of the matrix.  It is up to the implementing class to determine the
         * numerical tolerance for rank calculations.
         */
        virtual unsigned int rank() const = 0;

        /** Returns the vector \f$x\f$ that solves \f$Ax = b\f$, where the method is invoked on
         * matrix `A`.  This is, notionally, \f$A^{-1}b\f$, but most matrix libraries offer more
         * efficient solution methods than calculating an inverse.
         */
        virtual Ref solve(const MatrixImpl &b) const = 0;

        /** Returns the vector \f$x\f$ that solves the least-squares problem, that is, it is the
         * \f$x\f$ that minimizes \f$||Ax - b||\f$, where the calling matrix is `A`.  (In the normal
         * least squares terminology, `A` is \f$X\f$, `b` is \f$y\f$, and `x` is \f$\beta\f$).
         *
         * The default implementation simply returns `(A.transpose() * A).solve(A.transpose() * b)`
         * (except that A.tranpose() is called only once), but implementing classes should
         * override with some more efficient version if available (for example, the Eigen library
         * has an excellent such solver in its JacobiSVD class).
         */
        virtual Ref solveLeastSquares(const MatrixImpl &b) const;

        /** Returns the squared norm of the matrix.  If a vector, this is the squared L2-norm; for a
         * matrix, the squared Frobenius norm.
         */
        virtual double squaredNorm() const = 0;

        /** Returns true if the matrix is invertible, false if not. */
        virtual bool invertible() const = 0;

        /** Returns the inverse of the matrix.  Note that calling `solve()` is preferable when the
         * inverse is only a part of a calculation and not the intended value.  Calling this on a
         * non-invertible matrix could raise an exception, but might also return a matrix with
         * undefined coefficients, so you should generally check invertible() first.
         */
        virtual Ref inverse() const = 0;

        /** Returns the matrix `L` from the Cholesky decomposition of the matrix, where `L` is
         * lower-triangular and \f$LL^\top\f$ equals the called-upon matrix.
         */
        virtual Ref choleskyL() const = 0;

        /** Converts the matrix to a string representation using the given formatting parameters.
         *
         * \param precision the precision of values (as in `cout << std::setprecision(x)`).
         * \param coeffSeparator the separator between values on the same row.
         * \param rowSeparator the separator between two rows.
         * \param rowPrefix the prefix to print at the beginning of every row.
         *
         * The default values are defined in the Matrix class.
         */
        virtual std::string str(int precision, const std::string &coeffSeparator, const std::string &rowSeparator, const std::string &rowPrefix) const;
};

}}
