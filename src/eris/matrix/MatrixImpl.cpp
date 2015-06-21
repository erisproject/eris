#include <eris/matrix/MatrixImpl.hpp>
#include <sstream>
#include <iomanip>
#include <vector>

namespace eris { namespace matrix {

MatrixImpl::Ref MatrixImpl::clone() const {
    auto c = create(rows(), cols());
    *c = *this;
    return c;
}

MatrixImpl::Ref MatrixImpl::operator+(const MatrixImpl &b) const {
    auto c = clone();
    *c += b;
    return c;
}

MatrixImpl::Ref MatrixImpl::operator-(const MatrixImpl &b) const {
    auto c = clone();
    *c -= b;
    return c;
}

MatrixImpl::Ref MatrixImpl::operator*(double d) const {
    auto c = clone();
    *c *= d;
    return c;
}

MatrixImpl::Ref MatrixImpl::solveLeastSquares(const MatrixImpl &b) const {
    auto t = transpose();
    return (*t * *this)->solve(*(*t * b));
}

std::string MatrixImpl::str(int precision, const std::string &coeffSeparator, const std::string &rowSeparator, const std::string &rowPrefix) const {
    std::ostringstream out;
    // We need two passes: the first finds the maximum width of each column
    std::vector<unsigned int> widths(cols(), 1);
    for (unsigned int c = 0; c < cols(); c++) {
        unsigned int max = 1;
        for (unsigned int r = 0; r < rows(); r++) {
            std::ostringstream test;
            test.precision(precision);
            test << get(r, c);
            unsigned int len = test.str().length();
            if (len > max) max = len;
        }
        widths[c] = max;
    }

    for (unsigned int r = 0; r < rows(); r++) {
        if (r > 0) out << rowSeparator;
        out << rowPrefix;
        for (unsigned int c = 0; c < cols(); c++) {
            if (c > 0) out << coeffSeparator;
            out << std::setw(widths[c]) << std::setprecision(precision) << get(r, c);
        }
    }
    return out.str();
}

}}
