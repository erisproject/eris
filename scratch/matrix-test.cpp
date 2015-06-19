#include <memory>
#include <iostream>
#include <ostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <eris/Matrix.hpp>
#include <eris/matrix/EigenImpl.hpp>

using namespace eris;
using namespace eris::matrix;

#define PRINT(a) std::cout << __FILE__ << ":" << __LINE__ << ": " << #a << " = " << a << "\n"

class User {
    public:
        Matrix m;
        Matrix b1;
        Matrix b2;
        User(const Matrix &init) : m(init) {}
        void foo() {
    PRINT("hi");
            Eigen::MatrixXd m2(5, 2);
    PRINT("hi");
            m2 <<
                 12,  14,
                 21,   7,
                -18, -38,
                -32,  40,
                -44, -44;

    PRINT("hi");
            Matrix X = Matrix::create<EigenImpl>(std::move(m2));

    PRINT("hi");
            Eigen::VectorXd m3(5);
    PRINT("hi");
            m3 << 1, 2, 3, 4, 5;
    PRINT("hi");
            Matrix y = Matrix::create<EigenImpl>(std::move(m3));
    PRINT("hi");

            m += X;

    PRINT("hi");
            b1 = m.solveLeastSquares(y);
    PRINT("hi");
            b2 = (m.transpose() * m).solve(m.transpose() * y);
    PRINT("hi");
        }
};


int main() {
    Eigen::MatrixXd start = Eigen::MatrixXd::Zero(5, 2);
    PRINT("hi");
    User u(Matrix::create<EigenImpl>(start));
    PRINT("hi");
    u.foo();
    PRINT("hi");
    PRINT(u.b1.transpose().str()) << std::endl;
    PRINT(u.b2.transpose().str()) << std::endl;
    u.foo();
    u.foo();
    u.foo();
    PRINT(u.b1.transpose().str()) << std::endl;
    PRINT(u.b2.transpose().str()) << std::endl;

    Eigen::MatrixXd foo(3, 2);
    foo << 1, 0, 2.5, 1.597, 3000.42449, 1;
    std::cout << "print test:\n" << Matrix::create<EigenImpl>(foo) << "\n";
    std::cout << "eigen prints:\n" << foo << "\n";

    // Block testing
    Matrix blockbase = Matrix::create<EigenImpl>(5, 5);
    for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) {
        blockbase(r, c) = 100 + 10*r + c;
    }

    std::cout << "Block base:\n" << blockbase << "\n";

    Matrix blockmiddle = blockbase.block(1, 1, 3, 3);
    blockbase *= 2;

    std::cout << "Block middle:\n" << blockmiddle << "\n";

    // Set middle 3x3 to identity
    blockmiddle = blockbase.identity(3);

    std::cout << "Middle I3: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    PRINT(blockmiddle.rows());
    PRINT(blockmiddle.cols());

    // Set center to -1
    blockmiddle.block(1, 1, 1, 1)(0, 0) = -1;

    std::cout << "Middle -1: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    PRINT("hi");
    PRINT(blockbase.row(2).rows());
    PRINT(blockbase.row(2).cols());
    // Set right-most, second row to 1e100
    blockbase.row(1)[4] = 1e100;

    PRINT("hi");
    // Set (3,2) to -1e50
    blockmiddle.row(2).col(1)[0] = -1e50;

    std::cout << "big ones: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    // Divide columns 3 and 5 by 8
    blockbase.col(2) /= 8;
    blockbase.block(0, 3) /= 8;

    std::cout << "/8 right: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    // 0 out middle row/column
    blockbase.block(2, 0, -2) *= 0;
    blockbase.block(0, 2, 0, -2) *= 0;
    
    std::cout << "0 cross: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    Matrix save = blockbase;

    // Bottom two rows to all 5s, top 3 row to all 7s, left 1 row to all 3s, and right 3 columns of
    // bottom 3-row matrix to all 9s.  Should end up with:
    // 3 7 7 7 7
    // 3 7 7 7 7
    // 3 7 9 9 9
    // 3 5 9 9 9
    // 3 5 9 9 9
    blockbase.bottom(2) = blockbase.create(2, 5, 5);
    blockbase.top(3) = blockbase.create(3, 5, 7);
    blockbase.left(1) = blockbase.createVector(5, 3);
    blockbase.bottom(3).right(3) = blockbase.create(3, 3, 9);

    std::cout << "odd numbers: Block base:\n" << blockbase << "\n" << "Block middle:\n" << blockmiddle << "\n";

    std::cout << "save\n" << save << "\n";
}
