// IncrementalBuyer test script

// Test cases:
//
// I. Single good
//     (a) u(x) = x
//     (b) u(x) = √x̅
//     (c) u(x) = x²
// Expect all x in every case.
//
// II. Linear: u(x,y) = 2x + y
//     (a) px = 1, py = 1  (expect all x)
//     (b) px = 1, py = 6  (expect all x)
//     (c) px = 6, py = 1  (expect all y)
//     (d) px = 6, py = 6  (expect all x)
//
// III. Cobb-Douglas: u(x,y,z) = x^a y^b z^c
// Expect (a/(a+b+c)) px x = (b/(a+b+c)) py y = (c/(a+b+c)) pz z (i.e. spending
// on each good weighted by the exponent on each good)
//     (a) px = 1, py = 1, pz = 1; a=b=c=1
//     (b) px = 6, py = 1, pz = 1; a=b=1, c=2
//     (c) px = 1, py = 1, pz = 6; a=0, b=1, c=3
//     (d) px = 1, py = 6, pz = 6; a=1, b=2/3, c=1/3
//
// IV. Bliss:
//     (a) u(x,y,z) = c
//     (b) u(x,y,z) = c - x - y - z
//     (c) u(x,y,z) = c - xyz
// Expect no spending.
//
// V. Leontief: u(x,y,z) = min{x, 2y} + z
//     (0) px = 1, py = 6, pz = 6  (expect all spending on x=2y, with z=0)
//         NB: IncrementalBuyer can't actually handle this well, because each
//         increment will spend equally on x and y, which isn't x=2y, so
//         sometimes z is a better option.
//     (a) px = 1, py = 6, u = min{x, 2y}
//         (Needs large number of iterations to get close to x=2y)
//     (b) px = 1, py = 1, pz = 1  (expect all spending on z, with x=y=0)
//
// VI. Numeraire: u(x,y,m) = c + m + 5x - x²/2 + 4y - y²/2
//     (a) px = 1, py = 6  (expect x=4, y=0)
//     (b) px = 6, py = 1  (expect x=0, y=3)
//     (c) px = 1, py = 1  (expect x=4, y=3)
//     (d) px = 6, py = 6  (expect x=0, y=0)
//     (e) px = 1, py = 1, income = 2  (expect x = 1.5, y = 0.5)
//     (f) px = 1, py = 1, income = 6.9 (expect x = 3.95, y= 2.95)
//
// VII. u(x,y,z,m) = m + α(x + y + z) - β/2 (x² + y² + z²) - γ(xy + xz + yz)
// (Based on UBB paper: https://imaginary.ca/papers/2012/ubb/paper.pdf)
// Assumptions: α > β + pi, i ∈ {x,y,z}
//              β > γ
// Expect: x = α/(β+2γ) - (β+γ) / ((β+2γ)(β-γ)) px + γ / ((β+2γ)(β-γ)) (py + pz)
//
#include <eris/Eris.hpp>
#include <eris/Bundle.hpp>
#include <eris/Simulation.hpp>
#include <eris/consumer/Polynomial.hpp>
#include <eris/consumer/Quadratic.hpp>
#include <eris/consumer/CobbDouglas.hpp>
#include <eris/consumer/Compound.hpp>
#include <eris/intraopt/IncrementalBuyer.hpp>
#include <eris/market/Bertrand.hpp>
#include <cmath>
#include <gtest/gtest.h>

using namespace eris;
using namespace eris::market;
using namespace eris::consumer;
using namespace eris::firm;
using namespace eris::intraopt;


#define SETUP_SIM \
    Eris<Simulation> sim; \
    \
    auto m = sim->createGood<Good::Continuous>("Money"); \
    auto x = sim->createGood<Good::Continuous>("x"); \
    auto y = sim->createGood<Good::Continuous>("y"); \
    auto z = sim->createGood<Good::Continuous>("z"); \
    \
    Bundle m1(m, 1); \
    Bundle m6(m, 6); \
    Bundle x1(x, 1); \
    Bundle y1(y, 1); \
    Bundle z1(z, 1); \
    \
    auto fX1 = sim->createAgent<PriceFirm>(x1, m1); \
    auto fX6 = sim->createAgent<PriceFirm>(x1, m6); \
    auto fY1 = sim->createAgent<PriceFirm>(y1, m1); \
    auto fY6 = sim->createAgent<PriceFirm>(y1, m6); \
    auto fZ1 = sim->createAgent<PriceFirm>(z1, m1); \
    auto fZ6 = sim->createAgent<PriceFirm>(z1, m6); \
    \
    /* Create 6 test markets, for {px,py,pz}={1,6} \
     * NB: Can't add these to the simulation because different tests need different markets. */ \
    auto mX1 = Bertrand(x1, m1); mX1.addFirm(fX1); \
    auto mX6 = Bertrand(x1, m1); mX6.addFirm(fX6); \
    auto mY1 = Bertrand(y1, m1); mY1.addFirm(fY1); \
    auto mY6 = Bertrand(y1, m1); mY6.addFirm(fY6); \
    auto mZ1 = Bertrand(z1, m1); mZ1.addFirm(fZ1); \
    auto mZ6 = Bertrand(z1, m1); mZ6.addFirm(fZ6); \
    \
    int rounds = 0;


TEST(Case01_OneGood, Linear) {
    SETUP_SIM;

    auto con = sim->createAgent<Polynomial>();
    con->coef(x, 1) = 1; // u(x) = x

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);

    // Give the agents some income:
    con->assets() += 100 * m1;

    sim->cloneMarket(mX1); // px=1

    opt->reset();
    while (opt->optimize()) { ++rounds; }
    EXPECT_EQ(100, rounds);
    EXPECT_EQ(100*x1, con->assets());
    EXPECT_DOUBLE_EQ(100, con->currUtility());
}
TEST(Case01_OneGood, Sqrt) {
    SETUP_SIM;

    auto con = sim->createAgent<CobbDouglas>(x, 0.5);
    // con: u(x) = sqrt(x)
    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);

    // Give the agents some income:
    con->assets() += 150 * m1;

    opt->reset();

    sim->cloneMarket(mX1); // px=1

    while (opt->optimize()) { ++rounds; }
    EXPECT_EQ(100, rounds);
    EXPECT_EQ(150*x1, con->assets());
    EXPECT_DOUBLE_EQ(sqrt(150), con->currUtility());
}
TEST(Case01_OneGood, Squared) {
    SETUP_SIM;

    auto con = sim->createAgent<Polynomial>();
    con->coef(x, 2) = 1; // u(x) = x^2
    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);
    //auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);

    // Give the agents some income:
    con->assets() += 180 * m1;

    opt->reset();

    sim->cloneMarket(mX6); // px=6

    while (opt->optimize()) { ++rounds; }
    EXPECT_EQ(100, rounds);
    EXPECT_NEAR(30, con->assets()[x], 1e-12);
    EXPECT_EQ(0, con->assets() - Bundle(x, con->assets()[x]));
    EXPECT_NEAR(900, con->currUtility(), 1e-11);
}

#define SETUP_CASE2 \
    SETUP_SIM; \
    auto con = sim->createAgent<Polynomial>(); \
    con->coef(x, 1) = 2; \
    con->coef(y, 1) = 1; \
    con->assets() += 100 * m1; \
    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100); \
    opt->reset();

TEST(Case02_Linear, Px1_Py1) {
    SETUP_CASE2;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(100, rounds);
    EXPECT_EQ(100*x1, con->assets());
    EXPECT_DOUBLE_EQ(200, con->currUtility());
}
TEST(Case02_Linear, Px1_Py6) {
    SETUP_CASE2;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(100, rounds);
    EXPECT_EQ(100*x1, con->assets());
    EXPECT_DOUBLE_EQ(200, con->currUtility());
}
TEST(Case02_Linear, Px6_Py1) {
    SETUP_CASE2;

    sim->cloneMarket(mX6);
    sim->cloneMarket(mY1);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(100, rounds);
    EXPECT_EQ(100*y1, con->assets());
    EXPECT_DOUBLE_EQ(100, con->currUtility());
}
TEST(Case02_Linear, Px6_Py6) {
    SETUP_CASE2;

    sim->cloneMarket(mX6);
    sim->cloneMarket(mY6);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(100, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(100.0/6.0, a.remove(x), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(100.0/3.0, con->currUtility(), 1e-13);
}

TEST(Case03_CobbDouglas, Px1_Py1_Pz1__a1_b1_c1) {
    SETUP_SIM;

    auto con = sim->createAgent<CobbDouglas>(x, 1.0, y, 1.0, z, 1.0);
    con->assets() += 300*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 600);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ1);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(600, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(100, a.remove(x), 1e-12);
    EXPECT_NEAR(100, a.remove(y), 1e-12);
    EXPECT_NEAR(100, a.remove(z), 1e-12);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(100*100*100, con->currUtility(), 1e-7);
}
TEST(Case03_CobbDouglas, Px6_Py1_Pz1__a1_b1_c2) {
    SETUP_SIM;

    auto con = sim->createAgent<CobbDouglas>(x, 1, y, 1, z, 2);
    con->assets() += 300*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 600);
    opt->permuteThreshold(0.5);
    opt->reset();

    sim->cloneMarket(mX6);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ1);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(600, rounds);
    Bundle a = con->assetsB();
    // These can be off by quite a bit, since the algorithm typically works by
    // alternating between them at each step.  In the above, this won't happen
    // because it's crafted the exactly and equally split the purchase on every
    // iteration (it has equal prices and equal utility weights)
    EXPECT_NEAR(12.5, a.remove(x), 0.2);
    EXPECT_NEAR(75,   a.remove(y), 0.2);
    EXPECT_NEAR(150,  a.remove(z), 0.2);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(12.5*75*150*150, con->currUtility(), 100); // 100 isn't that big (utility ~ 21 million)
}
TEST(Case03_CobbDouglas, Px1_Py1_Pz6__a0_b1_c3) {
    SETUP_SIM;

    auto con = sim->createAgent<CobbDouglas>(x, 0, y, 1, z, 3);
    con->assets() += 300*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 600);
    opt->permuteThreshold(0.5);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ6);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(600, rounds);
    Bundle a = con->assetsB();
    // Should get 1/4 of spending on y, 3/4 on z, none on x
    EXPECT_EQ(0, a[x]);
    EXPECT_NEAR(75,   a.remove(y), 1e-13);
    EXPECT_NEAR(37.5,  a.remove(z), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(75*pow(37.5, 3), con->currUtility(), 1e-13);
}
TEST(Case03_CobbDouglas, Px1_Py6_Pz6__a1_b23_c13) {
    SETUP_SIM;

    auto con = sim->createAgent<CobbDouglas>(x, 1, y, 2.0/3, z, 1.0/3);
    con->assets() += 300*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 600);
    opt->permuteThreshold(0.5);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);
    sim->cloneMarket(mZ6);

    while (opt->optimize()) { ++rounds; }

    EXPECT_EQ(600, rounds);
    Bundle a = con->assetsB();
    // Should get 1/2 of spending on x, 1/3 on y, 1/6 on z
    double goodX, goodY, goodZ;
    EXPECT_NEAR(goodX = 300.0*(1.0/2.0),     a.remove(x), 1e-12);
    EXPECT_NEAR(goodY = 300.0*(1.0/3.0)/6.0, a.remove(y), 1e-12);
    EXPECT_NEAR(goodZ = 300.0*(1.0/6.0)/6.0, a.remove(z), 1e-12);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(goodX * pow(goodY, 2.0/3) * pow(goodZ, 1.0/3), con->currUtility(), 1e-11);
}


TEST(Case04_Bliss, Constant) {
    SETUP_SIM;

    auto con = sim->createAgent<Polynomial>(-13.0);

    con->assets() += 123*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 15);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);
    sim->cloneMarket(mZ6);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(0, rounds);
    EXPECT_EQ(123*m1, con->assets());
    EXPECT_EQ(-13, con->currUtility());
}
TEST(Case04_Bliss, ConstantMinusEach) {
    SETUP_SIM;

    auto con = sim->createAgent<Polynomial>(5.0);
    con->coef(x, 1) = -1;
    con->coef(y, 1) = -1;
    con->coef(z, 1) = -1;

    con->assets() += m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ1);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(0, rounds);
    EXPECT_EQ(m1, con->assets());
    EXPECT_EQ(5.0, con->currUtility());
}
TEST(Case04_Bliss, ConstantMinusProd) {
    SETUP_SIM;

    //auto con = sim->createAgent<Consumer::Simple>([&](const BundleNegative &b) { return -3.0-b[x]*b[y]*b[z]; });
    auto con = sim->createAgent<CompoundSum>(new Polynomial(-3), new CobbDouglas(x, 1, y, 1, z, 1, -1.0));

    con->assets() += 3*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 15);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);
    sim->cloneMarket(mZ6);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(0, rounds);
    EXPECT_EQ(3*m1, con->assets());
    EXPECT_EQ(-3.0, con->currUtility());
}

TEST(Case05_Leontief, Px1_Py6) {
    SETUP_SIM;

    auto con = sim->createAgent<Consumer::Simple>([&](const BundleNegative &b) {
            return fmin(b[x], 2*b[y]);
            });

    con->assets() += 14*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 1000);
    opt->permuteZeros(true);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(1000, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(3.50, a.remove(x), 1e-13);
    EXPECT_NEAR(1.75, a.remove(y), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(3.5, con->currUtility(), 1e-13);
}
TEST(Case05_Leontief, Px1_Py1_Pz1) {
    SETUP_SIM;

    auto con = sim->createAgent<Consumer::Simple>([&](const BundleNegative &b) {
            return fmin(b[x], 2*b[y]) + b[z];
            });

    con->assets() += 14*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 100);
    opt->permuteZeros(true);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ1);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(100, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(14, a.remove(z), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(14, con->currUtility(), 1e-13);
}

#define SETUP_CASE6 \
    SETUP_SIM; \
    auto con = sim->createAgent<Quadratic>(7.5); \
    con->coef(m) = 1; \
    con->coef(x) = 5; \
    con->coef(y) = 4; \
    con->coef(x, x) = -0.5; \
    con->coef(y, y) = -0.5; \
    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m); \
    opt->reset();

TEST(Case06_Numeraire, Px1_Py6) {
    SETUP_CASE6;

    con->assets() += 100*m1;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(4, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(4, a.remove(x), 1e-13);
    EXPECT_NEAR(96, a.remove(m), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(7.5 + 96 + 5*4 - 4*4/2, con->currUtility(), 1e-13);
}
TEST(Case06_Numeraire, Px6_Py1) {
    SETUP_CASE6;

    con->assets() += 100*m1;

    sim->cloneMarket(mX6);
    sim->cloneMarket(mY1);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(3, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(3, a.remove(y), 1e-13);
    EXPECT_NEAR(97, a.remove(m), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(7.5 + 97 + 4*3 - 3*3/2.0, con->currUtility(), 1e-15);
}
TEST(Case06_Numeraire, Px1_Py1) {
    SETUP_CASE6;

    con->assets() += 100*m1;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(7, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(4, a.remove(x), 1e-13);
    EXPECT_NEAR(3, a.remove(y), 1e-13);
    EXPECT_NEAR(93, a.remove(m), 13-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(7.5 + 93 + 5*4 - 4*4/2 + 4*3 - 3*3/2.0, con->currUtility(), 1e-15);
}
TEST(Case06_Numeraire, Px6_Py6) {
    SETUP_CASE6;

    con->assets() += 100*m1;

    sim->cloneMarket(mX6);
    sim->cloneMarket(mY6);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(0, rounds);
    EXPECT_EQ(100*m1, con->assetsB());
    EXPECT_EQ(107.5, con->currUtility());
}
TEST(Case06_Numeraire, Px1_Py1_I2) {
    SETUP_CASE6;

    con->assets() += 2*m1;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(100, rounds);
    Bundle a = con->assetsB();
    EXPECT_NEAR(1.5, a.remove(x), 1e-14);
    EXPECT_NEAR(0.5, a.remove(y), 1e-14);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(7.5 + 5*1.5 - 1.5*1.5/2 + 4*0.5 - 0.5*0.5/2, con->currUtility(), 1e-14);
}
TEST(Case06_Numeraire, Px1_Py1_I69) {
    SETUP_CASE6;

    con->assets() += 6.9*m1;

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);

    // Using the default 100 rounds won't let us get all those close: we'll spend .069 each time,
    // and the closest we can get to optimal with that is 57*.069 units of x, 43*.069 units of y.

    while (opt->optimize()) ++rounds;

    EXPECT_EQ(100, rounds);
    Bundle a = con->assetsB();
    const double expX = 0.57*6.9, expY = 0.43*6.9;
    EXPECT_NEAR(expX, a.remove(x), 1e-13);
    EXPECT_NEAR(expY, a.remove(y), 1e-13);
    EXPECT_EQ(0, a);
    EXPECT_NEAR(7.5 + 5*expX - expX*expX/2 + 4*expY - expY*expY/2, con->currUtility(), 1e-13);

    // We can cheat a little bit by using 138: this will result in increments of 0.05, which should
    // allow IncrementalBuyer to get within numerical precision error of 3.95/2.95.
    con->assets() = 6.9*m1;
    int rounds2 = 0;
    auto opt2 = sim->createIntraOpt<IncrementalBuyer>(con, m, 138);
    opt2->reset();
    while (opt2->optimize()) ++rounds2;

    EXPECT_EQ(138, rounds2);
    Bundle b = con->assetsB();
    EXPECT_NEAR(3.95, b.remove(x), 1e-13);
    EXPECT_NEAR(2.95, b.remove(y), 1e-13);
    EXPECT_EQ(0, b);
    EXPECT_NEAR(7.5 + 5*3.95 - 3.95*3.95/2 + 4*2.95 - 2.95*2.95/2, con->currUtility(), 1e-13);
}

TEST(Case07_UBB, Test1) {
    SETUP_SIM;

    double alpha = 1000, beta = 20, gamma = 5;

    auto con = sim->createAgent<Quadratic>();
    con->coef(m) = 1;
    con->coef(x) = alpha;
    con->coef(y) = alpha;
    con->coef(z) = alpha;
    con->coef(x, x) = -beta/2;
    con->coef(y, y) = -beta/2;
    con->coef(z, z) = -beta/2;
    con->coef(x, y) = -gamma;
    con->coef(x, z) = -gamma;
    con->coef(y, z) = -gamma;

    con->assets() += 200*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 2000);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY1);
    sim->cloneMarket(mZ1);

    opt->permuteAll();
    while (opt->optimize()) ++rounds;

    double denom1 = (beta + 2*gamma) * (beta - gamma);

    EXPECT_EQ(999, rounds);
    Bundle b = con->assetsB();
    EXPECT_NEAR(b.remove(x), alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 1
            + gamma / denom1 * (1 + 1), 1e-11);
    EXPECT_NEAR(b.remove(y), alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 1
            + gamma / denom1 * (1 + 1), 1e-11);
    EXPECT_NEAR(b.remove(z), alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 1
            + gamma / denom1 * (1 + 1), 1e-11);
    EXPECT_NEAR(b.remove(m), 100.1, 1e-11);

    EXPECT_NEAR(50100.05, con->currUtility(), 1e-11);
}

/* The below, with different prices, doesn't find the optimum, not because IncrementalBuyer is broken,
 * but rather because a stepwise approach (without being able to refund values) doesn't work: what
 * happens is that x and z (both p=1) get ramped up quickly, overshooting their optimal values
 * because, while y is fixed at 0, their marginal utilities are higher than they will be once y
 * takes on positive values.  Later choices then increment y, and the so optimum that gets found
 * involes negative marginal utilities on x and z, and y having a marginal utility of 6 (which is
 * correct).
 *
 * This is not really a bug, however: that's how IncrementalBuyer is *supposed* to work: hence "simple".
 */

TEST(Case07_UBB, Test2) {
    SETUP_SIM;

    // Try with a different parameterization
    double alpha = 100, beta = 1, gamma = 0.8;

    auto con = sim->createAgent<Quadratic>();
    con->coef(m) = 1;
    con->coef(x) = alpha;
    con->coef(y) = alpha;
    con->coef(z) = alpha;
    con->coef(x, x) = -beta/2;
    con->coef(y, y) = -beta/2;
    con->coef(z, z) = -beta/2;
    con->coef(x, y) = -gamma;
    con->coef(x, z) = -gamma;
    con->coef(y, z) = -gamma;

    con->assets() = 300*m1;

    auto opt = sim->createIntraOpt<IncrementalBuyer>(con, m, 3000);
    opt->reset();

    sim->cloneMarket(mX1);
    sim->cloneMarket(mY6); // changed
    sim->cloneMarket(mZ1);

    opt->permuteAll();
    while (opt->optimize()) ++rounds;

    double denom1 = (beta + 2*gamma) * (beta - gamma);

    // These are what x,y,z should be, if the consumer was perfectly optimizing:
    /*
    double trueX = alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 1
            + gamma / denom1 * (6 + 1);
    double trueY = alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 6
            + gamma / denom1 * (1 + 1);
    double trueZ = alpha/(beta+2*gamma) - (beta+gamma) / denom1 * 1
            + gamma / denom1 * (1 + 6);
    double trueM = 300 - 1*trueX - 6*trueY - 1*trueZ;
    */

    double simpX = 54.35, simpZ = 54.35, simpY = 211.0/30;
    double simpM = 300 - 1*simpX - 6*simpY - 1*simpZ;

    EXPECT_EQ(1509, rounds);
    Bundle b = con->assetsB();
    EXPECT_NEAR(simpX, b.remove(x), 1e-11);
    EXPECT_NEAR(simpY, b.remove(y), 1e-11);
    EXPECT_NEAR(simpZ, b.remove(z), 1e-11);
    EXPECT_NEAR(simpM, b.remove(m), 1e-11);

    EXPECT_NEAR(simpM + alpha*(simpX+simpY+simpZ) - beta/2*(simpX*simpX + simpY*simpY + simpZ*simpZ)
            - gamma*(simpX*simpY + simpX*simpZ + simpY*simpZ), con->currUtility(), 1e-11);

//    std::cout << "\n\n";
//    std::cout << "optimized bundle:  " << con->assets() << "\n";
//    std::cout << "optimized utility: " << con->currUtility() << "\n";
//    std::cout << "optimal? bundle:   " << Bundle({{x,wantX}, {y,wantY}, {z,wantZ}, {m,wantM}}) << "\n";
//    std::cout << "optimal? utility:  " << con->utility(Bundle {{x,wantX}, {y,wantY}, {z,wantZ}, {m,wantM}}) << "\n";
//    std::cout << "du/dx: " << con->d(con->assets(), x) << "\n";
//    std::cout << "du/dy: " << con->d(con->assets(), y) << "\n";
//    std::cout << "du/dz: " << con->d(con->assets(), z) << "\n";
//    std::cout << "du/dm: " << con->d(con->assets(), m) << "\n";
//
//    std::cout << "du/dx: " << con->d(con->assets() - x1, x) << "\n";
//    std::cout << "du/dy: " << con->d(con->assets() - y1, y) << "\n";
//    std::cout << "du/dz: " << con->d(con->assets() - z1, z) << "\n";
//    std::cout << "du/dm: " << con->d(con->assets() - m1, m) << "\n";
//    EXPECT_NEAR(1, con->currUtility(), 1e-10);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
