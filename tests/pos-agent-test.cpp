// Test script for testing the various boundary and wrapping handling of
// Positional<Agent> and WrappedPositional<Agent>.

#include <eris/Positional.hpp>
#include <eris/WrappedPositional.hpp>
#include <eris/Agent.hpp>
#include <gtest/gtest.h>
#include <iostream>

#define INF std::numeric_limits<double>::infinity()
#define EXPECT_EQP(a,b) EXPECT_EQ(Position(a), b)

using namespace eris;

TEST(Construction, Unbounded) {
    Positional<Agent> p1({1});
    WrappedPositional<Agent> p2({2.5}, {INF}, {-INF});

    EXPECT_EQP({1.0}, p1.position());
    EXPECT_EQP({2.5}, p2.position());
    EXPECT_EQP({INF}, p1.upperBound());
    EXPECT_EQP({INF}, p2.upperBound());
    EXPECT_EQP({-INF}, p1.lowerBound());
    EXPECT_EQP({-INF}, p2.lowerBound());

    EXPECT_FALSE(p1.bounded());
    EXPECT_FALSE(p1.bindingLower());
    EXPECT_FALSE(p1.bindingUpper());
    EXPECT_FALSE(p1.binding());

    EXPECT_FALSE(p2.bounded());
    EXPECT_FALSE(p2.bindingLower());
    EXPECT_FALSE(p2.bindingUpper());
    EXPECT_FALSE(p2.binding());
}

TEST(Construction, Bounded) {
    Positional<Agent>  p1a({1}, {3}, {0});
    Positional<Agent>  p1b({1}, {0}, {3});
    Positional<Agent>  p1c({1}, {INF}, {0});
    Positional<Agent>  p1d({1}, {-INF}, {3});
    Positional<Agent>  p1e({3}, {-INF}, {3});
    Positional<Agent>  p1f({0}, {0}, {3});
    WrappedPositional<Agent> p2a({1}, {3}, {0});
    WrappedPositional<Agent> p2b({1}, {0}, {3});
    WrappedPositional<Agent> p2c({1}, {INF}, {0});
    WrappedPositional<Agent> p2d({1}, {-INF}, {3});

    EXPECT_EQP({1.0}, p1a.position());
    EXPECT_EQP({1.0}, p2c.position());

    EXPECT_TRUE(p1a.bounded());
    EXPECT_TRUE(p1b.bounded());
    EXPECT_TRUE(p1c.bounded());
    EXPECT_TRUE(p1d.bounded());
    EXPECT_TRUE(p1e.bounded());
    EXPECT_TRUE(p1f.bounded());

    EXPECT_FALSE(p1a.bindingUpper());
    EXPECT_FALSE(p1b.bindingUpper());
    EXPECT_FALSE(p1c.bindingUpper());
    EXPECT_FALSE(p1d.bindingUpper());
    EXPECT_TRUE(p1e.bindingUpper());
    EXPECT_FALSE(p1f.bindingUpper());
}

TEST(Boundaries, Boundaries) {
    Positional<Agent> p1({1}, {3}, {0});
    EXPECT_THROW(p1.moveBy({-2}), PositionalBoundaryError);

    Positional<Agent> p2({1}, {3}, {0});
    EXPECT_THROW(p2.moveBy({2.01}), PositionalBoundaryError);

    Positional<Agent> p3({1}, {3}, {0});
    EXPECT_NO_THROW(p3.moveBy({2}));
    EXPECT_EQP({3}, p3.position());

    EXPECT_TRUE(p3.bindingUpper());
    EXPECT_FALSE(p3.bindingLower());
    EXPECT_TRUE(p3.binding());

    WrappedPositional<Agent> p4({1,2,3,4,5}, {0,0,0,0,0}, {5,5,5,5,5}, {});
    EXPECT_NO_THROW(p4.moveTo({4,1,2,5,0}));
    EXPECT_TRUE(p4.binding());
    EXPECT_TRUE(p4.bindingUpper());
    EXPECT_TRUE(p4.bindingUpper());

    EXPECT_THROW(p4.moveBy({1.00001,0,0,0,0}), PositionalBoundaryError);
    EXPECT_THROW(p4.moveBy({0,0,0,0.00000000001,0}), PositionalBoundaryError);
    EXPECT_THROW(p4.moveBy({0,-1.0001,0,0,0}), PositionalBoundaryError);
    EXPECT_NO_THROW(p4.moveBy({-4,4,-2,0,0}));

    EXPECT_EQ(Position({0,5,0,5,0}), p4.position());
}

TEST(Wrapping, Circle) {
    WrappedPositional<Agent> p1({1}, {-1}, {5});

    EXPECT_NO_THROW(p1.moveTo({19}));
    EXPECT_EQP({1}, p1.position());

    WrappedPositional<Agent> p2({1}, {-1.25}, {5.25});
    EXPECT_NO_THROW(p2.moveBy({800000}));
    EXPECT_EQP({0.5}, p2.position());

    EXPECT_NO_THROW(p2.moveBy({-44.7578125}));
    EXPECT_EQP({1.2421875}, p2.position());

    p2.moveBy({0.0078125});
    EXPECT_FALSE(p2.binding());
    EXPECT_FALSE(p2.bindingUpper());
    EXPECT_FALSE(p2.bindingLower());
}

TEST(Wrapping, Donut) {
    WrappedPositional<Agent> p1({1,1}, {-2,-2}, {3,3}); // Wrap in both dimensions

    EXPECT_NO_THROW(p1.moveBy({99,99}));
    EXPECT_EQ(Position({0,0}), p1.position());

    EXPECT_NO_THROW(p1.moveBy({-51.390625, 56.703125}));
    EXPECT_EQ(Position({-1.390625, 1.703125}), p1.position());


    WrappedPositional<Agent> p2({2.25, 0}, {-10,-10}, {10,10});

    p1.moveTo({-1.5, -1});
    EXPECT_EQ(std::hypot(1.25, 1), p1.distance(p2)); // wrap x
    EXPECT_EQ(std::hypot(3.75, 1), p2.distance(p1)); // no wrap

    p1.moveTo({-0.125, 3});
    EXPECT_EQ(std::hypot(2.375, 2), p1.distance(p2)); // wrap y
    EXPECT_EQ(std::hypot(2.375, 3), p2.distance(p1)); // no wrap

    p1.moveTo({-1.875, 2.9375});
    EXPECT_EQ(std::hypot(0.875, 2.0625), p1.distance(p2)); // wrap both
    EXPECT_EQ(std::hypot(4.125, 2.9375), p2.distance(p1)); // no wrapping
}

TEST(Wrapping, Cylinder) {
    // x doesn't wrap, y does
    WrappedPositional<Agent> p1({1,1}, {-5,-5}, {-1,-1}, {1});

    // x=1 is outside the boundary, but we're explicitly allowed to do that.
    EXPECT_EQ(Position({1,-3}), p1.position());

    EXPECT_NO_THROW(p1.moveBy({-3, 99}));

    EXPECT_THROW(p1.moveBy({99,0}), PositionalBoundaryError);

    EXPECT_EQ(Position({-2, -4}), p1.position());

    p1.moveTo({-5, -1});
    EXPECT_EQ(Position({-5, -1}), p1.position());
    EXPECT_TRUE(p1.bindingLower());
    EXPECT_TRUE(p1.binding());
    EXPECT_FALSE(p1.bindingUpper());

    p1.moveTo({-1,-5});
    EXPECT_EQ(Position({-1,-5}), p1.position());
    EXPECT_FALSE(p1.bindingLower());
    EXPECT_TRUE(p1.binding());
    EXPECT_TRUE(p1.bindingUpper());
}

//TEST(Wrapping, Partial

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

