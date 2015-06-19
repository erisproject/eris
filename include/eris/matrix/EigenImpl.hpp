#pragma once
#include <eris/matrix/MatrixImpl.hpp>
#include <Eigen/Core>
#include <Eigen/QR>
#include <Eigen/SVD>

namespace eris { namespace matrix {

/** Eigen matrix implementation.  Note that in order to use this class, you will generally need to
 * include additional compilation flags to tell your compiler where to find Eigen. */
class EigenImpl : public MatrixImpl {
    public:
        /** Creates an uninitialized matrix of the requested size. */
        EigenImpl(unsigned int rows, unsigned int cols) : matrix_(new Eigen::MatrixXd(rows, cols)), m(*matrix_) {
            std::cerr << "(r,c) constructor called\n";
        }
        /** Creates a matrix with values copied from the given matrix. */
        explicit EigenImpl(const Eigen::MatrixXd &init) : matrix_(new Eigen::MatrixXd(init)), m(*matrix_) {
            std::cerr << "matrix copy constructor called\n";
        }
        /** Creates a matrix using the given rvalue matrix. */
        explicit EigenImpl(Eigen::MatrixXd &&init) : matrix_(new Eigen::MatrixXd(std::move(init))), m(*matrix_) {
            std::cerr << "matrix move constructor called\n";
        }
        /** Creates a matrix view from a copy of the given matrix block. */
        explicit EigenImpl(const Eigen::Block<Eigen::Ref<Eigen::MatrixXd>> &block) : block_(new Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>(block)), m(*block_) {
            std::cerr << "block copy constructor called\n";
        }
        /** Creates a matrix view from the given matrix block rvalue reference. */
        explicit EigenImpl(Eigen::Block<Eigen::Ref<Eigen::MatrixXd>> &&block) : block_(new Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>(std::move(block))), m(*block_) {
            std::cerr << "block move constructor called\n";
        }
//        /** Creates a matrix view from a copy of the given matrix block. */
//        explicit EigenImpl(const std::unique_ptr<Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>> &block) : block_(new Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>(block)), m(*block_) {
//            std::cerr << "block copy constructor called\n";
//        }
//        /** Creates a matrix view from the given matrix block pointer rvalue reference. */
//        explicit EigenImpl(std::unique_ptr<Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>> &&block) : block_(std::move(block)), m(*block_) {
//            std::cerr << "block move constructor called\n";
//        }
        /** Returns a copy of the current matrix. */
        virtual Ref clone() const override { return wrap(Eigen::MatrixXd(m)); }
        /** Returns the number of rows of this matrix. */
        virtual unsigned int rows() const { return m.rows(); }
        /** Returns the number of columns of this matrix. */
        virtual unsigned int cols() const { return m.cols(); }
        /** Accesses an element of this matrix. */
        virtual const double& get(unsigned int r, unsigned int c) const override {
            return m(r, c);
        }
        /** Sets a value of this matrix. */
        virtual void set(unsigned int r, unsigned int c, double d) override {
            m(r, c) = d;
            resetCache();
        }
        /** Creates a new matrix of the requested size. Coefficients are allocated but uninitialized. */
        virtual Ref create(unsigned int rows, unsigned int cols) const override {
            return wrap(rows, cols);
        }
        /** Creates a new matrix of the requested size, with each coefficient initialized to the given value. */
        virtual Ref create(unsigned int rows, unsigned int cols, double initial) const override {
            return wrap(Eigen::MatrixXd::Constant(rows, cols, initial));
        }
        /** Creates an identify matrix of the requested size. */
        virtual Ref identity(unsigned int size) const override {
            return wrap(Eigen::MatrixXd::Identity(size, size));
        }

        /** Creates a view of this matrix. */
        virtual Ref block(unsigned int rowOffset, unsigned int colOffset, unsigned int nRows, unsigned int nCols) const override {
            std::cerr << "block called, constructing...\n";
            return wrap(const_cast<EigenImpl&>(*this).m.block(rowOffset, colOffset, nRows, nCols));
//            return wrap(std::unique_ptr<Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>>(new Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>(
//                            const_cast<EigenImpl&>(*this).m.block(rowOffset, colOffset, nRows, nCols))));
        }

        /** Copies the coefficients from another matrix into this matrix. */
        virtual void operator=(const MatrixImpl &b) override  { std::cerr << "assigning " << mat(b) << "\n"; m = mat(b); resetCache(); }
        /** Adds another matrix to this matrix. */
        virtual void operator+=(const MatrixImpl &b) override { m += mat(b); resetCache(); }
        /** Subtracts another matrix from this matrix. */
        virtual void operator-=(const MatrixImpl &b) override { m -= mat(b); resetCache(); }
        /** Scales this matrix by a constant. */
        virtual void operator*=(double d) override { m *= d; resetCache(); }
        /** Adds this matrix to another matrix, returning the result. */
        virtual Ref operator+(const MatrixImpl &b) const override {
            return wrap(m + mat(b));
        }
        /** Subtracts another matrix from this matrix without changing this matrix, returning the result. */
        virtual Ref operator-(const MatrixImpl &b) const override {
            return wrap(m - mat(b));
        }
        /** Matrix multiplication.  The result is returned. */
        virtual Ref operator*(const MatrixImpl &b) const override {
            return wrap(m * mat(b));
        }
        /** Matrix scaling by a constant; the result is returned. */
        virtual Ref operator*(double d) const override {
            return wrap(m * d);
        }
        /** Returns the transpose of this matrix. */
        virtual Ref transpose() const override {
            return wrap(m.transpose());
        }
        /** Returns the rank of this matrix. */
        virtual unsigned int rank() const override {
            return qr().rank();
        }
        /** Returns the vector `x` that solves \f$A x = b\f$ for given matrix `b`. */
        virtual Ref solve(const MatrixImpl &b) const override {
            return wrap(qr().solve(mat(b)));
        }
        /** Returns the vector `x` that minimizes \f$||Ax - b||\f$.  This is calculated more
         * efficiently (both numerically and computationally) than calling `(A.transpose() *
         * A).solve(A.transpose() * y)`.
         */
        virtual Ref solveLeastSquares(const MatrixImpl &b) const override {
            return wrap(jacobiSvd().solve(mat(b)));
        }
        /** Returns the squared norm of the matrix. */
        virtual double squaredNorm() const override {
            return m.squaredNorm();
        }

        /** Returns true if the matrix is invertible. */
        virtual bool invertible() const override {
            return qr().isInvertible();
        }
        /** Returns the inverse of this matrix.  If the matrix is not invertible, the values of the
         * returned matrix are not defined.
         */
        virtual Ref inverse() const override {
            return wrap(qr().inverse());
        }
        /** Returns the lower-triangular "L" matrix of the Cholesky decomposition of this matrix,
         * where \f$LL^\top = A\f$.
         */
        virtual Ref choleskyL() const override {
            return wrap(llt().matrixL());
        }

        /** Resets the cached qr() and jacobiSvd() decompositions, if set.  This method is called
         * internally whenever the matrix is changed, and should also be called if the matrix is
         * manipulated externally.
         */
        void resetCache() {
            qr_.release();
            jacobisvd_.release();
            llt_.release();
        }

        /** Accesses the Eigen FullPivHouseholderQR associated with this matrix.  If the
         * decomposition has not yet been done, it is calculated when this method is first called;
         * subsequent calls reuse the calculated value until the matrix is changed.
         *
         * The returned reference is invalidated when resetCache() is called (whether explicitly or
         * implicitly by mutating operations).
         */
        const Eigen::FullPivHouseholderQR<Eigen::MatrixXd>& qr() const {
            if (!qr_) qr_.reset(new Eigen::FullPivHouseholderQR<Eigen::MatrixXd>(m));
            return *qr_;
        }

        /** Accesses the JacobiSVD decomposition associated with this matrix suitable for
         * least-squares solving.  If the decomposition has not yet been done, it is calculated
         * first; subsequent calls reuse the calculated value until the matrix is changed.
         *
         * The returned reference is invalidated when resetCache() is called (whether explicitly or
         * implicitly by mutating operations).
         */
        const Eigen::JacobiSVD<Eigen::MatrixXd> jacobiSvd() const {
            if (!jacobisvd_) jacobisvd_.reset(new Eigen::JacobiSVD<Eigen::MatrixXd>(m, Eigen::ComputeThinU | Eigen::ComputeThinV));
            return *jacobisvd_;
        }

        /** Accesses the LLT decomposition object associated with this matrix.  If the decomposition
         * has not yet been done, it is performed when called.  Like the methods above, the object
         * is reset when the underlying matrix is changed.
         *
         * The returned reference is invalidated when resetCache() is called (whether explicitly or
         * implicitly by mutating operations).
         */
        const Eigen::LLT<Eigen::MatrixXd> llt() const {
            if (!llt_) {
                llt_.reset(new Eigen::LLT<Eigen::MatrixXd>());
                llt_->compute(m);
            }
            return *llt_;
        }

    private:
        // If this class has its own matrix, it's stored here, and `matrix` is a reference to this.
        std::unique_ptr<Eigen::MatrixXd> matrix_;
        // If this class is a view ("block") of another matrix, the block is stored here and
        // `matrix` is a reference to this.
        std::unique_ptr<Eigen::Block<Eigen::Ref<Eigen::MatrixXd>>> block_;

    public:
        /** The Eigen matrix (or matrix-like object) reference.  Note: if modifying directly (that
         * is, not through the operators and methods of this class), you must also call
         * `obj.resetCache()` to reset any cached decompositions or else the various decomposition
         * methods (such as solve(), rank(), and inverse()) will not work correctly.
         */
        Eigen::Ref<Eigen::MatrixXd> m;

    private:

        static const Eigen::Ref<Eigen::MatrixXd>& mat(const MatrixImpl &other) { return static_cast<const EigenImpl&>(other).m; }
        /// Wraps a unique_ptr + upcast with a constructor of this class
        template <typename... Args>
        static Ref wrap(Args&&... args) {
            return Ref(static_cast<MatrixImpl*>(new EigenImpl(std::forward<Args>(args)...)));
        }

        // The various cached decompositions of `matrix`
        mutable std::unique_ptr<Eigen::FullPivHouseholderQR<Eigen::MatrixXd>> qr_;
        mutable std::unique_ptr<Eigen::JacobiSVD<Eigen::MatrixXd>> jacobisvd_;
        mutable std::unique_ptr<Eigen::LLT<Eigen::MatrixXd>> llt_;

};

}}
