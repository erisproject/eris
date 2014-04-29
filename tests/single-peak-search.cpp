#include <eris/algorithms.hpp>
#include <gtest/gtest.h>

using eris::single_peak_search;

TEST(Maximize, Quadratic) {
    auto f = [] (const double &x) -> double { return -3*x*x + 14*x - 3; };

    EXPECT_NEAR(14.0/6.0, single_peak_search(f, -10, 10), 2e-8);
    EXPECT_NEAR(14.0/6.0, single_peak_search(f, -100, 3.0), 2e-8);
    EXPECT_NEAR(14.0/6.0, single_peak_search(f, 0, 100000), 2e-8);
    EXPECT_NEAR(14.0/6.0, single_peak_search(f, 2.3, 2.4), 3e-9);
}

TEST(Maximize, Quartic) {
    auto f = [] (const double &x) -> double { const double x2 = x*x; return -21237*x2*x2 + 13*x2 - 1247*x + 3; };

    EXPECT_NEAR(-0.24526910870656568, single_peak_search(f, -10, 10), 2e-9);
    EXPECT_NEAR(-0.24526910870656568, single_peak_search(f, -1, 0), 4e-9);
    EXPECT_NEAR(-0.24526910870656568, single_peak_search(f, -1e10, 1e10), 7e-10);
    EXPECT_NEAR(-0.24526910870656568, single_peak_search(f, -0.246, 0.245), 2e-9);
}

TEST(Maximize, LeftEndPoint) {
    auto f = [] (const double &x) -> double { return 100 - 12*x; };

    // NB: not using DOUBLE_EQ here, these should be exact matches
    EXPECT_EQ(-14.675, single_peak_search(f, -14.675, 10000));
    EXPECT_EQ(-12, single_peak_search(f, -12, -1));
    EXPECT_EQ(2000, single_peak_search(f, 2000, 50000));
}

TEST(Maximize, CubicRightEndPoint) {
    auto f = [] (const double &x) -> double { const double x2 = x*x; return x2*x - 2*x2 + 3*x + 17; };

    // NB: not using DOUBLE_EQ here, these should be exact matches
    EXPECT_EQ(10000, single_peak_search(f, -14.675, 10000));
    EXPECT_EQ(-1, single_peak_search(f, -12, -1));
    EXPECT_EQ(1e100, single_peak_search(f, -1e100, 1e100));
}

TEST(Maximize, PosQuadraticEnd) {
    auto f = [] (const double &x) -> double { return x*x + 14*x + 70; };

    // We should always end up at one end-point or the other, but which side depends on the initial
    // range.  NB: the above has a minimum at -7, but we should never get that back.
    EXPECT_EQ(10, single_peak_search(f, -10, 10));
    EXPECT_EQ(-11, single_peak_search(f, -11, -6));
    EXPECT_EQ(-2.875, single_peak_search(f, -11, -2.875));
}
