#include <eris/matrix/MatrixImpl.hpp>

namespace eris { namespace matrix {

#define NULLIMPL_THROW { throw std::logic_error("Error: NullImpl matrix implementation cannot be used for Matrix operations!"); }

/** Null matrix implementation.  The implementation simply throws exceptions if attempted to be used
 * in any way.  The only thing that doesn't throw an exception is calling rows() or cols(): both
 * return 0.  This class is simply intended so that methods can contain "null" default arguments.
 */
class NullImpl : public MatrixImpl {
    protected:
        /// Returns 0
        virtual unsigned int rows() const override { return 0; }
        /// Returns 0
        virtual unsigned int cols() const override { return 0; }
        /// Returns true
        virtual bool null() const override { return true; }
        // Hide everything here from doxygen; these are really methods that shouldn't ever be
        // called.
        /// \cond
        virtual const double& get(unsigned, unsigned) const override NULLIMPL_THROW
        virtual void set(unsigned, unsigned, double) override NULLIMPL_THROW
        virtual Ref create(unsigned, unsigned) const override NULLIMPL_THROW
        virtual Ref create(unsigned, unsigned, double) const override NULLIMPL_THROW
        virtual void resize(unsigned, unsigned) override NULLIMPL_THROW
        virtual Ref identity(unsigned) const override NULLIMPL_THROW
        virtual Ref block(unsigned, unsigned, unsigned, unsigned) const override NULLIMPL_THROW
        virtual void operator=(const MatrixImpl&) override NULLIMPL_THROW
        virtual void operator+=(const MatrixImpl&) override NULLIMPL_THROW
        virtual void operator-=(const MatrixImpl&) override NULLIMPL_THROW
        virtual Ref operator*(const MatrixImpl&) const override NULLIMPL_THROW
        virtual void operator*=(double) override NULLIMPL_THROW
        virtual Ref transpose() const override NULLIMPL_THROW
        virtual unsigned int rank() const override NULLIMPL_THROW
        virtual Ref solve(const MatrixImpl&) const override NULLIMPL_THROW
        virtual double squaredNorm() const override NULLIMPL_THROW
        virtual bool invertible() const override NULLIMPL_THROW
        virtual Ref inverse() const override NULLIMPL_THROW
        virtual Ref choleskyL() const override NULLIMPL_THROW
        /// \endcond
};

}}

#undef NULLIMPL_THROW
