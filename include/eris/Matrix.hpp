#pragma once
#include <eris/matrix/MatrixImpl.hpp>
#include <eris/matrix/NullImpl.hpp>

namespace eris {

/** This namespace contains the generic matrix interface wrapper (MatrixImpl) and any specific
 * matrix library implementations.
 */
namespace matrix {}

// Forward declarations:
class Vector;
class RowVector;

/** This class contains the basic matrix functionality that is required within eris.  It is designed
 * without requiring any particular matrix implementation: rather it uses an instanced subclass of
 * the MatrixImpl class which provides the actual functionality referenced here.  This does not aim
 * to be a complete matrix interface at all--rather it provides only the matrix operations needed by
 * other parts of eris.
 *
 * Note that every Matrix object is backed by an instance of a subclass of MatrixImpl, and that
 * using two Matrix objects together which have different implementing subclasses is not permitted
 * (but is not actually checked, and thus will most likely cause a segmentation fault).
 *
 * A new Matrix object can be constructed either by explicitly using an implementation class via the
 * templated `create<Implementation>(...)` method, or can be spawned from an existing Matrix object
 * using the non-templated create() methods or identity() method, each of which creates a matrix
 * backed by the same implementation class.
 */
class Matrix {
    public:
        /** The default constructor constructs a null matrix (that is, a Matrix using a NullImpl
         * implementation).  Such a matrix cannot be used for any matrix operations: attempting to
         * do so will throw a std::logic_error exception.
         */
        Matrix();

        /// Copy constructor.  The underlying implementation object is cloned.
        Matrix(const Matrix& copy);

        /// Move constructor.  The underlying implementation object is moved.
        Matrix(Matrix&& move) = default;

        /// Virtual destructor.
        virtual ~Matrix() = default;

        /** Returns the number of rows of this matrix. */
        unsigned int rows() const;

        /** Returns the number of columns of this matrix. */
        unsigned int cols() const;

        /** Returns true if this is a default-constructed matrix. */
        bool null() const;

        /** Creates a new Matrix object backed by the given implementation.  Arguments given are
         * passed to the implementation class constructor.
         *
         * For example, Matrix m = Matrix::create<MyImplementation>(... constructor arguments ...)
         */
        template <class Implementation, typename... Args, typename = typename std::enable_if<std::is_base_of<matrix::MatrixImpl, Implementation>::value>>
        static Matrix create(Args&&... args) { return Matrix(matrix::MatrixImpl::Ref(new Implementation(std::forward<Args>(args)...))); }

        /** Creates a new Matrix object backed by the same implementation class as the current one.
         * Matrix elements may be left uninitialized.
         */
        Matrix create(unsigned int rows, unsigned int cols) const;

        /** Creates a new Matrix object backed by the same implementation class the current one, and
         * initializes all elements to the given value.
         */
        Matrix create(unsigned int rows, unsigned int cols, double initial) const;

        /** Just like `create(rows, cols)`, but return a pointer to an object created with `new`.
         * The caller is responsible for destruction of the returned object.
         */
        Matrix* newMatrix(unsigned int rows, unsigned int cols) const;

        /** Just like `create(rows, cols, initial)`, but return a pointer to an object created with
         * `new`.  The caller is responsible for destruction of the returned object.
         */
        Matrix* newMatrix(unsigned int rows, unsigned int cols, double initial) const;

        /** Creates a new Vector object backed by the same implementation class as the current one.
         * Vector coefficients may be left uninitialized.
         */
        Vector createVector(unsigned int rows) const;

        /** Creates a new Vector object backed by the same implementation class as the current one.
         * Vector coefficients will all be initialized to the given value.
         */
        Vector createVector(unsigned int rows, double initial) const;

        /** Just like `createVector(rows)`, but return a pointer to an object created with `new`.
         * The caller is responsible for destruction of the returned object.
         */
        Vector* newVector(unsigned int rows) const;

        /** Just like `createVector(rows, initial)`, but return a pointer to an object created with
         * `new`.  The caller is responsible for destruction of the returned object.
         */
        Vector* newVector(unsigned int rows, double initial) const;

        /** Creates a new RowVector object backed by the same implementation class as the current one.
         * RowVector coefficients may be left uninitialized.
         */
        RowVector createRowVector(unsigned int cols) const;

        /** Creates a new RowVector object backed by the same implementation class as the current one.
         * RowVector coefficients will all be initialized to the given value.
         */
        RowVector createRowVector(unsigned int cols, double initial) const;

        /** Just like `createRowVector(cols)`, but return a pointer to an object created with `new`.
         * The caller is responsible for destruction of the returned object.
         */
        RowVector* newRowVector(unsigned int cols) const;

        /** Just like `createRowVector(cols, initial)`, but return a pointer to an object created
         * with `new`.  The caller is responsible for destruction of the returned object.
         */
        RowVector* newRowVector(unsigned int cols, double initial) const;

        /** Creates a new square identity matrix of the given size.
         */
        Matrix identity(unsigned int size) const;

        /** Returns a new matrix view object that represents a block of this matrix.  Adjusting the
         * returned object adjusts this matrix, and vice versa.  If the current matrix is
         * runtime-constant (which typically means it is itself a view from a const Matrix), the
         * returned Matrix will also be runtime-constant.
         *
         * \param rowOffset the row index of this matrix corresponding to row 0 of the returned
         * matrix view.
         * \param colOffset the column index of this matrix corresponding to row 0 of the returned
         * matrix view.
         * \param rows the number of rows the matrix view should have, if positive, in which case
         * `rowOffset + rows` must be no greater than the source matrix's `rows()`.  If 0 or
         * negative, this is the (negative) number of rows to leave off the right-hand-side, in
         * which case `rowOffset - rows` must be no greater than the source matrix's `rows()`.  The
         * default, 0, means "use all rows from the given rowOffset to the end".  -2 would leave off
         * the last two rows, etc.
         * \param cols the number of columns the matrix view should have.  Works just like `rows`,
         * but for columns.
         *
         * \throws std::logic_error if the parameters are impossible to fulfill, in other words, if
         * they request a matrix with 0 or negative rows/columns, or they request a matrix view that
         * extends beyond the limits of this matrix.
         */
        Matrix block(unsigned int rowOffset, unsigned int colOffset, int rows = 0, int cols = 0);

        /** Const version of block.  This differs from the above in that the returned Matrix is
         * *always* runtime-constant: any attempt to mutate the returned matrix will throw an
         * exception.
         */
        Matrix block(unsigned int rowOffset, unsigned int colOffset, int rows = 0, int cols = 0) const;

        /** Returns a matrix view of the first `rows` rows of this matrix.  If an offset is given,
         * the first `offset` rows are skipped. */
        Matrix top(unsigned int rows, unsigned int offset = 0);

        /** Returns a matrix view of the first `rows` rows of this matrix.  If an offset is given,
         * the first `offset` rows are skipped. */
        Matrix top(unsigned int rows, unsigned int offset = 0) const;

        /** Returns a matrix view of the last `rows` rows of this matrix.  If an offset is given,
         * the last `offset` rows are skipped. */
        Matrix bottom(unsigned int rows, unsigned int offset = 0);

        /** Returns a matrix view of the last `rows` rows of this matrix.  If an offset is given,
         * the last `offset` rows are skipped. */
        Matrix bottom(unsigned int rows, unsigned int offset = 0) const;

        /** Returns a matrix view of the first `cols` columns of this matrix.  If an offset is
         * given, the first `offset` columns are skipped. */
        Matrix left(unsigned int cols, unsigned int offset = 0);

        /** Returns a matrix view of the first `cols` columns of this matrix.  If an offset is
         * given, the first `offset` columns are skipped. */
        Matrix left(unsigned int cols, unsigned int offset = 0) const;

        /** Returns a matrix view of the last `cols` columns of this matrix.  If an offset is given,
         * the last `offset` columns are skipped. */
        Matrix right(unsigned int cols, unsigned int offset = 0);

        /** Returns a matrix view of the last `cols` columns of this matrix.  If an offset is given,
         * the last `offset` columns are skipped. */
        Matrix right(unsigned int cols, unsigned int offset = 0) const;

        /** Resizes a matrix.  When the new size requires new coefficients, they will be
         * uninitialized.  This will fail under various conditions:
         *
         * - if the current matrix is actually a RowVector or Vector object and the number of rows
         *   or columns, respectively, is changed.
         * - if either `rows` or `cols` is 0 (resizing to a null matrix is not permitted)
         * - if the current object is a matrix block view rather than a base matrix
         *
         * Note that resizing a matrix may invalidate block views derived from the matrix; callers
         * should take care that such blocks are not held over a resize operation.
         */
        virtual void resize(unsigned int rows, unsigned int cols);

        /** Returns a new matrix view object that represents a single row of this matrix.  `row(3)` is
         * equivalent to `block(3, 0, 1, cols())`, but the return is wrapped in a RowVector subclass
         * instead of merely a Matrix object.
         */
        RowVector row(unsigned int row);

        /// const version of row()
        RowVector row(unsigned int row) const;

        /** Returns a new matrix view object that represents a single column of this matrix.
         * `col(3)` is equivalent to `block(0, 3, rows(), 1)`, but the return is wrapped in a Vector
         * subclass instead of merely a Matrix object.
         */
        Vector col(unsigned int col);

        /// const version of col()
        Vector col(unsigned int col) const;

        /** Assigns the values of matrix `b` to this matrix.  If, and only if, the current matrix is
         * a NullImpl matrix (typically caused by a default-constructed Matrix object), then this
         * method operates as a copy constructor, cloning the passed-in object; otherwise this
         * invokes the assignment operator on the underlying implementation.
         */
        Matrix& operator=(const Matrix &b);

        /** Moves the value of matrix `b` into this matrix.  If this matrix is default-constructed,
         * this simply moves the given matrix implementation object into this one (so that code like
         * `Matrix m = othermatrix.transpose()` works as expected).  Otherwise this invokes the copy
         * constructor.
         */
        Matrix& operator=(Matrix &&b);

        /** Adds two matrices together, returns a new Matrix containing the result.
         */
        Matrix operator+(const Matrix &b) const;

        /** Adds a matrix to this matrix and returns a reference to this (updated) matrix.
         */
        Matrix& operator+=(const Matrix &b);

        /** Returns the negative of this matrix.  This is simply an alias for multiplying the matrix
         * by -1.
         */
        Matrix operator-() const;
        
        /** Subtracts `b` from this matrix, returning the result in a new matrix.
         */
        Matrix operator-(const Matrix &b) const;

        /** Subtracts a matrix from this matrix and returns a reference to this (updated) matrix.
         */
        Matrix& operator-=(const Matrix &b);

        /** Scales a matrix by the given scalar value, returning the result in a new matrix.
         */
        template<typename Numeric, typename = std::enable_if<std::is_arithmetic<Numeric>::value>>
        Matrix operator*(Numeric d) const;

        /** Multiplies this matrix by the matrix `b`, returning the result in a new matrix.
         */
        Matrix operator*(const Matrix &b) const;

        /** Multiplies this matrix by the row `b`, returning the result in a new matrix.  Explicit
         * version for multiplying by a row (to prevent ambigious conversion to double).
         */
        Matrix operator*(const RowVector &b) const;

        /** Multiplies this matrix by the column `b`, returning the result in a new matrix.
         * Explicit version for multiplying by a column (to prevent ambigious conversion to double).
         */
        Matrix operator*(const Vector &b) const;

        /** Scales a matrix by the inverse of the given scalar value, returning the result in a new
         * matrix.
         */
        template<typename Numeric, typename = std::enable_if<std::is_arithmetic<Numeric>::value>>
        Matrix operator/(Numeric d) const;

        /** Scales a matrix by the given scalar value, returning the result in a new matrix.
         */
        template<typename Numeric>
        friend Matrix operator*(Numeric d, const Matrix &b);

        /** Mutator version of matrix scaling; return reference to this (updated) matrix.
         */
        template<typename Numeric, typename = std::enable_if<std::is_arithmetic<Numeric>::value>>
        Matrix& operator*=(Numeric d);

        /** Mutator multiplication by a matrix is explicitly disallowed. */
        Matrix& operator*=(const Matrix &m) = delete;

        /** Mutator version of matrix inverse scaling; return reference to this (updated) matrix.
         */
        template<typename Numeric, typename = std::enable_if<std::is_arithmetic<Numeric>::value>>
        Matrix& operator/=(Numeric d);

        /** Mutator division by a matrix is explicitly disallowed. */
        Matrix& operator/=(const Matrix &m) = delete;

        /** Proxy class to allow operations such as `matrix(a,b) += 2` */
        class CoeffProxy final {
            public:
                /** Using a CoeffProxy as a const double accesses the actual matrix coefficient. */
                operator const double&() const { return matrix.getCoeff(row, col); }
                /** Assigning a double assigns to the actual matrix coefficient. */
                double operator=(const double &d) { matrix.setCoeff(row, col, d); return d; }
                /** Assigning a CoeffProxy assigns the CoeffProxy's double value. */
                CoeffProxy& operator=(const CoeffProxy &other) { matrix.setCoeff(row, col, (const double&)other); return *this; }
#define ERIS_MATRIX_PROXY_MUT(OP) \
                double operator OP##= (const double &d) { matrix.setCoeff(row, col, matrix.getCoeff(row, col) OP d); return d; }
                /// Mutator addition for supporting code like `matrix(r,c) += 3`
                ERIS_MATRIX_PROXY_MUT(+)
                /// Mutator addition for supporting code like `matrix(r,c) -= 3`
                ERIS_MATRIX_PROXY_MUT(-)
                /// Mutator addition for supporting code like `matrix(r,c) *= 3`
                ERIS_MATRIX_PROXY_MUT(*)
                /// Mutator addition for supporting code like `matrix(r,c) /= 3`
                ERIS_MATRIX_PROXY_MUT(/)
#undef ERIS_MATRIX_PROXY_MUT
            private:
                friend class Matrix;
                friend class RowVector;
                friend class Vector;
                Matrix &matrix;
                unsigned int row, col;
                CoeffProxy(Matrix& m, unsigned int r, unsigned int c) : matrix(m), row(r), col(c) {}
                // Private copy-constructor (to prevent external copying)
                CoeffProxy(const CoeffProxy &copy) = default;
                // Private move-constructor (to prevent external moving)
                CoeffProxy(CoeffProxy &&copy) = default;
        };

        /** Accesses the coefficients of this matrix.  This returns a proxy object that can be used
         * as a double */
        const CoeffProxy operator()(unsigned int r, unsigned int c) const;

        /** Accesses the cofficients of this matrix (non-const version).  This returns a proxy
         * object that can be used as a double&, including assignment and compound assignment.
         */
        CoeffProxy operator()(unsigned int r, unsigned int c);

        /** Returns the transpose of this matrix. */
        Matrix transpose() const;

        /** Returns the rank of this matrix. */
        unsigned int rank() const;

        /** Returns the vector \f$x\f$ that solves \f$Ax = b\f$, where the method is invoked on
         * matrix `A`.  This is, notionally, \f$A^{-1}b\f$, but most matrix libraries offer more
         * efficient solution methods than calculating an inverse.
         */
        Matrix solve(const Matrix &b) const;

        /** Returns the vector \f$x\f$ that solves the least-squares problem, that is, it is the
         * \f$x\f$ that minimizes \f$||Ax - b||\f$, where the calling matrix is `A`.  (In the normal
         * least squares terminology, `A` is \f$X\f$, `b` is \f$y\f$, and `x` is \f$\beta\f$).
         *
         * This is, mathematically, \f$(A^\top A)^{-1} A^\top b\f$, but implementations typically
         * solve it more efficiently than performing those explicit calculations.
         */
        Matrix solveLeastSquares(const Matrix &b) const;

        /** Returns the squared norm of the matrix. For a vector, this is just the squared L2-norm;
         * for a matrix, the Frobenius norm.
         */
        double squaredNorm() const;

        /** Returns true if the matrix is invertible, false otherwise. */
        bool invertible() const;

        /** Returns the inverse of the matrix.  Note that calling `solve()` is preferable when the
         * inverse is only a part of a calculation and not the intended value.
         */
        Matrix inverse() const;

        /** Returns the matrix `L` from the Cholesky decomposition of the matrix, where `L` is
         * lower-triangular and \f$LL^\top\f$ equals the called-upon matrix.
         */
        Matrix choleskyL() const;

        /** Allows a matrix to be used as a double.  If the matrix is not 1x1, this throws an
         * std::logic_error exception.
         */
        operator const double&() const;

        /** Converts the matrix to a string representation using the given formatting parameters.
         *
         * \param precision the precision of values (as in `cout << std::setprecision(x)`).  Default 8.
         * \param coeffSeparator the separator between values on the same row (default `"  "`)
         * \param rowSeparator the separator between two rows (default `"\n"`)
         * \param rowPrefix the prefix to print at the beginning of every row (default blank)
         */
        std::string str(int precision = 8, const std::string &coeffSeparator = "  ", const std::string &rowSeparator = "\n", const std::string &rowPrefix = "") const;

        /** Converts the matrix to a string representation.  This simply calls str() with default
         * arguments.
         */
        operator std::string() const;

        /** Sending the matrix to an ostream sends the str() representation. */
        friend std::ostream& operator<<(std::ostream &os, const Matrix &A);

    protected:
        /** Protected constructor: constructs a matrix from an matrix implementation object.  Called
         * by the create() and related methods.
         */
        Matrix(matrix::MatrixImpl::Ref b);

        /** Accesses the implementation object. Will throw if const_ is set. */
        matrix::MatrixImpl& impl();

        /** Const access to the implementation object, usable for const methods.  To call this from
         * a non-const method, use constImpl() instead.
         */
        const matrix::MatrixImpl& impl() const;

        /** Explicit const access to the implementation object. */
        const matrix::MatrixImpl& constImpl() const;

        /** If true, the matrix is read-only.  Attempting to use non-const methods will fail. */
        bool const_ = false;

        /// Internal method to directly get a coefficient, used by CoeffProxy object
        const double& getCoeff(unsigned int r, unsigned int c) const;

        /// Internal method to directly set a coefficient, used for CoeffProxy object
        void setCoeff(unsigned int r, unsigned int c, double d);

        /// Internal method for creating a matrix view.
        Matrix block(unsigned int rOffset, unsigned int cOffset, int nRow, int nCol, bool constant) const;

        /** If true, this is a block view of another matrix.  If false, this is an actual matrix. */
        bool block_ = false;

    private:
        /** The implementation instance.  This is wrapped in a unique_ptr because MatrixImpl itself
         * is an abstract class.
         */
        matrix::MatrixImpl::Ref impl_;

};

/** This class extends Matrix for a column vector, that is, a Nx1 matrix.  In particular it adds a
 * subscript operator and hides the Matrix class's cols() method with one that returns always the
 * constant 1.
 *
 * Note that this is only a convenience wrapper around a Matrix with `cols() == 1`: it is, for all
 * intents and purposes, simply a matrix with a single column.
 *
 * Vector objects are created by calling Matrix.createVector().
 */
class Vector final : public Matrix {
    public:
        /// Default constructor that creates a null matrix; see Matrix().
        Vector();
        /** Creates a Vector from a Matrix.  Throws an exception if the Matrix is not a column
         * matrix.  This constructor is deliberately non-explicit so as to allow implicit conversion
         * from a Matrix to a Vector (with runtime failure if the matrix isn't actually a vector).
         */
        Vector(const Matrix &m) : Matrix(m) { if (m.cols() != 1) throw std::logic_error("Cannot convert non-column Matrix into Vector"); }
        /** Moves constructor from a Matrix.  Throws an exception if the Matrix is not a column
         * matrix.
         */
        Vector(Matrix &&m) : Matrix(std::move(m)) { if (Matrix::cols() != 1) throw std::logic_error("Cannot convert non-column Matrix into Vector"); }
        /// Returns 1.
        unsigned int cols() const { return 1; }
        /// Returns the size of this vector, i.e. the number of rows
        unsigned int size() const;
        /** Accesses the requested coefficient of this column.  This returns a proxy object that can
         * be used as a double */
        const CoeffProxy operator[](unsigned int r) const;

        /** Accesses the requested cofficient of this column (non-const version).  This returns a
         * proxy object that can be used as a double&, including assignment and compound assignment.
         */
        CoeffProxy operator[](unsigned int r);

        /** Assigns the values of matrix `b` to this matrix.  If, and only if, the current matrix is
         * a NullImpl matrix (typically caused by a default-constructed Matrix object), then this
         * method operates as a copy constructor, cloning the passed-in object; otherwise this
         * invokes the assignment operator on the underlying implementation.
         *
         * Throws an exception if b is not a column.
         */
        Vector& operator=(const Matrix &b);

        /** Moves the value of matrix `b` into this matrix by moving the underlying implementation
         * object instead of the actual values.  Throws an exception if b is not a column.
         */
        Vector& operator=(Matrix &&b);

        /** Returns a view of this vector containing just the first `n` elements.  If an offset is
         * given, the first `offset` elements are skipped. */
        Vector head(unsigned int n, unsigned int offset = 0);

        /** Returns a view of this vector containing just the first `n` elements.  If an offset is
         * given, the first `offset` elements are skipped. */
        Vector head(unsigned int n, unsigned int offset = 0) const;

        /** Returns a view of this vector containing just the last `n` elements.  If an offset is
         * given, the last `offset` elements are skipped. */
        Vector tail(unsigned int n, unsigned int offset = 0);

        /** Returns a view of this vector containing just the last `n` elements.  If an offset is
         * given, the last `offset` elements are skipped. */
        Vector tail(unsigned int n, unsigned int offset = 0) const;

        /** Resizes this vector to have the given length. */
        void resize(unsigned int length);

        /** Overrides Matrix::resize to fail if attempting to resize a Vector to something that
         * isn't a Vector.
         */
        virtual void resize(unsigned int rows, unsigned int cols) override;

    private:
        friend class Matrix;
        Vector(matrix::MatrixImpl::Ref b) : Matrix(std::move(b)) {}
};

/** This class extends Matrix for a row vector, that is, a 1xN matrix.  In particular it adds a
 * subscript operator and hides the Matrix class's rows() method with one that returns always the
 * constant 1.
 *
 * Note that this is only a convenience wrapper around a Matrix with `rows() == 1`: it is, for all
 * intents and purposes, simply a matrix with a single row.
 *
 * RowVector objects are created by calling Matrix.createRowVector().
 */
class RowVector final : public Matrix {
    public:
        /// Default constructor that creates a null matrix; see Matrix().
        RowVector();
        /** Creates a RowVector from a Matrix.  Throws an exception if the Matrix is not a row
         * matrix.  This constructor is deliberately non-explicit so as to allow implicit conversion
         * from a Matrix to a RowVector (with runtime failure if the matrix isn't actually a row
         * vector).
         */
        RowVector(const Matrix &m) : Matrix(m) { if (m.rows() != 1) throw std::logic_error("Cannot convert non-row Matrix into RowVector"); }
        /** Move constructor that creates a RowVector from a Matrix.  Throws an exception if the Matrix is
         * not a row matrix.
         */
        RowVector(Matrix &&m) : Matrix(std::move(m)) { if (Matrix::rows() != 1) throw std::logic_error("Cannot convert non-row Matrix into RowVector"); }
        /// Returns 1.
        unsigned int rows() const { return 1; }
        /// Returns the size of this vector, i.e. the number of columns
        unsigned int size() const;
        /** Accesses the requested coefficients of this row.  This returns a proxy object that can
         * be used as a double */
        const CoeffProxy operator[](unsigned int c) const;

        /** Accesses the requested cofficient of this row (non-const version).  This returns a proxy
         * object that can be used as a double&, including assignment and compound assignment.
         */
        CoeffProxy operator[](unsigned int c);

        /** Assigns the values of matrix `b` to this matrix.  If, and only if, the current matrix is
         * a NullImpl matrix (typically caused by a default-constructed Matrix object), then this
         * method operates as a copy constructor, cloning the passed-in object; otherwise this
         * invokes the assignment operator on the underlying implementation.
         *
         * Throws an exception if b is not a row.
         */
        RowVector& operator=(const Matrix &b);

        /** Moves the value of matrix `b` into this matrix by moving the underlying implementation
         * object instead of the actual values.  Throws an exception if b is not a row.
         */
        RowVector& operator=(Matrix &&b);

        /** Returns a view of this row vector containing just the first `n` elements.  If an offset
         * is given, the first `offset` elements are skipped. */
        RowVector head(unsigned int n, unsigned int offset = 0);

        /** Returns a view of this row vector containing just the first `n` elements.  If an offset
         * is given, the first `offset` elements are skipped. */
        RowVector head(unsigned int n, unsigned int offset = 0) const;

        /** Returns a view of this row vector containing just the last `n` elements.  If an offset
         * is given, the last `offset` elements are skipped. */
        RowVector tail(unsigned int n, unsigned int offset = 0);

        /** Returns a view of this row vector containing just the last `n` elements.  If an offset
         * is given, the last `offset` elements are skipped. */
        RowVector tail(unsigned int n, unsigned int offset = 0) const;

        /** Resizes this row vector to have the given length. */
        void resize(unsigned int length);

        /** Overrides Matrix::resize to fail if attempting to resize a RowVector to something that
         * isn't a RowVector.
         */
        virtual void resize(unsigned int rows, unsigned int cols) override;

        using Matrix::operator*;

    private:
        friend class Matrix;
        RowVector(matrix::MatrixImpl::Ref b) : Matrix(std::move(b)) {}
};

// Inlined methods:
inline Matrix::Matrix() : impl_(new matrix::NullImpl()) {}
inline Matrix::Matrix(const Matrix& copy) : impl_(copy.impl().clone()) {}
inline Matrix::Matrix(matrix::MatrixImpl::Ref b) : impl_(std::move(b)) {}
inline matrix::MatrixImpl& Matrix::impl() { return *impl_; }
inline const matrix::MatrixImpl& Matrix::impl() const { if (const_) throw std::runtime_error("Attempt to modify a const Matrix"); return *impl_; }
inline const matrix::MatrixImpl& Matrix::constImpl() const { return impl(); }
inline unsigned int Matrix::rows() const { return impl().rows(); }
inline unsigned int Matrix::cols() const { return impl().cols(); }
inline bool Matrix::null() const { return impl().null(); }
inline const Matrix::CoeffProxy Matrix::operator()(unsigned int r, unsigned int c) const { return CoeffProxy(const_cast<Matrix&>(*this), r, c); }
inline Matrix::CoeffProxy Matrix::operator()(unsigned int r, unsigned int c) { return CoeffProxy(*this, r, c); }
inline void Matrix::setCoeff(unsigned int r, unsigned int c, double d) { impl().set(r, c, d); }
inline const double& Matrix::getCoeff(unsigned int r, unsigned int c) const { return impl().get(r, c); }
inline Matrix Matrix::create(unsigned int rows, unsigned int cols) const { return Matrix(impl().create(rows, cols)); }
inline Matrix Matrix::create(unsigned int rows, unsigned int cols, double initial) const { return Matrix(impl().create(rows, cols, initial)); }
inline Matrix* Matrix::newMatrix(unsigned int rows, unsigned int cols) const { return new Matrix(impl().create(rows, cols)); }
inline Matrix* Matrix::newMatrix(unsigned int rows, unsigned int cols, double initial) const { return new Matrix(impl().create(rows, cols, initial)); }
inline Vector Matrix::createVector(unsigned int rows) const { return Vector(impl().create(rows, 1)); }
inline Vector Matrix::createVector(unsigned int rows, double initial) const { return Vector(impl().create(rows, 1, initial)); }
inline Vector* Matrix::newVector(unsigned int rows) const { return new Vector(impl().create(rows, 1)); }
inline Vector* Matrix::newVector(unsigned int rows, double initial) const { return new Vector(impl().create(rows, 1, initial)); }
inline RowVector Matrix::createRowVector(unsigned int cols) const { return RowVector(impl().create(1, cols)); }
inline RowVector Matrix::createRowVector(unsigned int cols, double initial) const { return RowVector(impl().create(1, cols, initial)); }
inline RowVector* Matrix::newRowVector(unsigned int cols) const { return new RowVector(impl().create(1, cols)); }
inline RowVector* Matrix::newRowVector(unsigned int cols, double initial) const { return new RowVector(impl().create(1, cols, initial)); }
inline Matrix Matrix::identity(unsigned int size) const { return Matrix(impl().identity(size)); }
inline Matrix& Matrix::operator=(const Matrix &b) {
    if (impl().null()) { if (!b.impl().null()) { impl_ = b.impl().clone(); } }
    else { impl() = b.impl(); }
    return *this;
}
inline Matrix& Matrix::operator=(Matrix &&b) {
    if (!impl().null()) return operator=(b);
    // Else we have a default constructed, so do move semantics:
    impl_ = std::move(b.impl_);
    return *this;
}
inline Matrix Matrix::operator+(const Matrix &b) const { return impl() + b.impl(); }
inline Matrix& Matrix::operator+=(const Matrix &b) { impl() += b.impl(); return *this; }
inline Matrix Matrix::operator-(const Matrix &b) const { return impl() - b.impl(); }
inline Matrix Matrix::operator-() const { return (*this) * -1; }
inline Matrix& Matrix::operator-=(const Matrix &b) { impl() -= b.impl(); return *this; }
inline Matrix Matrix::operator*(const Matrix &b) const { return impl() * b.impl(); }
inline Matrix Matrix::operator*(const Vector &b) const { return impl() * b.impl(); }
inline Matrix Matrix::operator*(const RowVector &b) const { return impl() * b.impl(); }
template<typename Numeric, typename>
inline Matrix Matrix::operator*(Numeric d) const { return impl() * (double) d; }
template<typename Numeric, typename>
inline Matrix Matrix::operator/(Numeric d) const { return impl() * (1.0/(double)d); }
// Doxygen can't figure out that these are the same as the ones in the class definition:
/// \cond
template<typename Numeric>
inline Matrix operator*(Numeric d, const Matrix &b) { return b * d; }
/// \endcond
template<typename Numeric, typename>
inline Matrix& Matrix::operator*=(Numeric d) { impl() *= (double) d; return *this; }
template<typename Numeric, typename>
inline Matrix& Matrix::operator/=(Numeric d) { impl() *= (1.0/(double)d); return *this; }
inline Matrix Matrix::transpose() const { return impl().transpose(); }
inline unsigned int Matrix::rank() const { return impl().rank(); }
inline Matrix Matrix::solve(const Matrix &b) const { return impl().solve(b.impl()); }
inline Matrix Matrix::solveLeastSquares(const Matrix &b) const { return impl().solveLeastSquares(b.impl()); }
inline double Matrix::squaredNorm() const { return impl().squaredNorm(); }
inline bool Matrix::invertible() const { return impl().invertible(); }
inline Matrix Matrix::inverse() const { return impl().inverse(); }
inline Matrix Matrix::choleskyL() const { return impl().choleskyL(); }
inline Matrix::operator const double&() const {
    if (rows() != 1 or cols() != 1) throw std::logic_error("Unable to convert non-1x1 matrix to double");
    return getCoeff(0, 0);
}
inline Matrix::operator std::string() const { return str(); }
inline std::string Matrix::str(int precision, const std::string &coeffSeparator, const std::string &rowSeparator, const std::string &rowPrefix) const {
    return impl().str(precision, coeffSeparator, rowSeparator, rowPrefix);
}
inline std::ostream& operator<<(std::ostream &os, const Matrix &A) { return os << A.str(); }

inline RowVector::RowVector() : Matrix() {}
inline unsigned int RowVector::size() const { return cols(); }
inline const Matrix::CoeffProxy RowVector::operator[](unsigned int c) const { return (*this)(0, c); }
inline Matrix::CoeffProxy RowVector::operator[](unsigned int c) { return (*this)(0, c); }
inline RowVector& RowVector::operator=(const Matrix &b) {
    if (b.rows() != 1) throw std::logic_error("Cannot assign non-row matrix to a RowVector vector");
    Matrix::operator=(b);
    return *this;
}
inline RowVector& RowVector::operator=(Matrix &&b) {
    if (b.rows() != 1) throw std::logic_error("Cannot move non-row matrix to a RowVector vector");
    Matrix::operator=(std::move(b));
    return *this;
}


inline Vector::Vector() : Matrix() {}
inline unsigned int Vector::size() const { return rows(); }
inline const Matrix::CoeffProxy Vector::operator[](unsigned int r) const { return (*this)(r, 0); }
inline Matrix::CoeffProxy Vector::operator[](unsigned int r) { return (*this)(r, 0); }
inline Vector& Vector::operator=(const Matrix &b) {
    if (b.cols() != 1) throw std::logic_error("Cannot assign non-column matrix to a Vector vector");
    Matrix::operator=(b);
    return *this;
}
inline Vector& Vector::operator=(Matrix &&b) {
    if (b.cols() != 1) throw std::logic_error("Cannot move non-column matrix to a Vector vector");
    Matrix::operator=(std::move(b));
    return *this;
}


inline Matrix Matrix::block(unsigned int rowOffset, unsigned int colOffset, int nRows, int nCols, bool constant) const {
    if (nRows <= 0) nRows += rows() - rowOffset;
    if (nCols <= 0) nCols += cols() - colOffset;

    if (nRows <= 0 or rowOffset + nRows > rows() or nCols <= 0 or colOffset + nCols > cols())
        throw std::logic_error("Cannot create a matrix block that exceeds matrix bounds or has no rows/columns");

    Matrix block(impl().block(rowOffset, colOffset, nRows, nCols));
    if (constant) block.const_ = true;
    block.block_ = true;
    return block;
}
inline Matrix Matrix::block(unsigned int rowOffset, unsigned int colOffset, int nRows, int nCols) {
    return block(rowOffset, colOffset, nRows, nCols, const_);
}
inline Matrix Matrix::block(unsigned int rowOffset, unsigned int colOffset, int nRows, int nCols) const {
    return block(rowOffset, colOffset, nRows, nCols, true);
}
inline Vector Matrix::col(unsigned int col)          { return block(0, col, rows(), 1, const_); }
inline Vector Matrix::col(unsigned int col) const    { return block(0, col, rows(), 1, true); }
inline RowVector Matrix::row(unsigned int row)       { return block(row, 0, 1, cols(), const_); }
inline RowVector Matrix::row(unsigned int row) const { return block(row, 0, 1, cols(), true); }

inline Matrix Matrix::top(unsigned int nrows, unsigned int offset)          { return block(offset, 0, nrows, cols(), const_); }
inline Matrix Matrix::top(unsigned int nrows, unsigned int offset) const    { return block(offset, 0, nrows, cols(), true); }
inline Matrix Matrix::bottom(unsigned int nrows, unsigned int offset)       { return block(rows()-nrows-offset, 0, nrows, cols(), const_); }
inline Matrix Matrix::bottom(unsigned int nrows, unsigned int offset) const { return block(rows()-nrows-offset, 0, nrows, cols(), true); }
inline Matrix Matrix::left(unsigned int ncols, unsigned int offset)         { return block(0, offset, rows(), ncols, const_); }
inline Matrix Matrix::left(unsigned int ncols, unsigned int offset) const   { return block(0, offset, rows(), ncols, true); }
inline Matrix Matrix::right(unsigned int ncols, unsigned int offset)        { return block(0, cols()-ncols-offset, rows(), ncols, const_); }
inline Matrix Matrix::right(unsigned int ncols, unsigned int offset) const  { return block(0, cols()-ncols-offset, rows(), ncols, true); }

inline Vector Vector::head(unsigned int n, unsigned int offset)       { return block(offset,          0, n, 1, const_); }
inline Vector Vector::head(unsigned int n, unsigned int offset) const { return block(offset,          0, n, 1, true); }
inline Vector Vector::tail(unsigned int n, unsigned int offset)       { return block(cols()-n-offset, 0, n, 1, const_); }
inline Vector Vector::tail(unsigned int n, unsigned int offset) const { return block(cols()-n-offset, 0, n, 1, true); }

inline RowVector RowVector::head(unsigned int n, unsigned int offset)       { return block(0,          offset, 1, n, const_); }
inline RowVector RowVector::head(unsigned int n, unsigned int offset) const { return block(0,          offset, 1, n, true); }
inline RowVector RowVector::tail(unsigned int n, unsigned int offset)       { return block(0, cols()-n-offset, 1, n, const_); }
inline RowVector RowVector::tail(unsigned int n, unsigned int offset) const { return block(0, cols()-n-offset, 1, n, true); }

inline void Matrix::resize(unsigned int rows, unsigned int cols) {
    if (rows == 0 or cols == 0) throw std::logic_error("Cannot resize a Matrix to a null matrix (rows == 0 or cols == 0)");
    if (block_) throw std::logic_error("Cannot resize a block view of another matrix");
    impl().resize(rows, cols);
}
inline void Vector::resize(unsigned int rows, unsigned int cols) {
    if (cols != 1) throw std::logic_error("Cannot resize a Vector to something that isn't a column");
    Matrix::resize(rows, cols);
}
inline void RowVector::resize(unsigned int rows, unsigned int cols) {
    if (rows != 1) throw std::logic_error("Cannot resize a RowVector to something that isn't a row");
    Matrix::resize(rows, cols);
}
inline void Vector::resize(unsigned int length) { resize(length, 1); }
inline void RowVector::resize(unsigned int length) { resize(1, length); }

}
