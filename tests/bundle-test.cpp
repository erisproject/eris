// Test script for testing various Bundle and BundleNegative behaviours
//
// NB: Take care to use numbers that are exactly representable in doubles when doing arithmetic
// manipulations: otherwise you get numerical imprecision resulting in things not being equal.  e.g.
// 0.1*0.2 != 0.02 in double math.  Stick to fractions representable with a denominator of a power
// of 2.
//
// Alternatively, use EXPECT_DOUBLE_EQ when appropriate instead of EXPECT_EQ

#include <limits>
#include <eris/Bundle.hpp>
#include <gtest/gtest.h>
#include <cmath>

using eris::eris_id_t;
using eris::Bundle;
using eris::BundleNegative;
using std::numeric_limits;

TEST(Construction, Empty) {
    BundleNegative a;
    Bundle b;
    // Get a BN cast of the Bundle
    BundleNegative *bn = (BundleNegative*) &b;

    // We don't test these until later, but just assume they work
    EXPECT_EQ(0, a.size());
    EXPECT_EQ(0, b.size());
    EXPECT_EQ(0, bn->size());

    EXPECT_TRUE(a.empty());
    EXPECT_TRUE(b.empty());
    EXPECT_TRUE(bn->empty());

    bool foundA = false, foundB = false, foundBN = false;
    for (auto z : a)   foundA = true;
    for (auto z : b)   foundB = true;
    for (auto z : *bn) foundBN = true;

    EXPECT_FALSE(foundA);
    EXPECT_FALSE(foundB);
    EXPECT_FALSE(foundBN);
}

TEST(Construction, Pair) {
    eris_id_t real_idA = 2, real_idB = 43, real_idC = 8949;
    double real_valA = -13.75, real_valB = 1.25, real_valC = 0;
    BundleNegative a(real_idA, real_valA);
    Bundle b(real_idB, real_valB);
    BundleNegative *bn = (BundleNegative*) &b;
    Bundle c(real_idC, real_valC);

    EXPECT_EQ(1, a.size());
    EXPECT_EQ(1, b.size());
    EXPECT_EQ(1, bn->size());
    EXPECT_EQ(1, c.size());

    EXPECT_FALSE(a.empty());
    EXPECT_FALSE(b.empty());
    EXPECT_FALSE(bn->empty());
    EXPECT_FALSE(c.empty());

    int foundA = 0, foundB = 0, foundBN = 0, foundC = 0;
    eris_id_t idA = 41241, idB = 55156, idBN = 424571, idC = 32;
    double valA = -123, valB = -456, valBN = -789, valC = -1012;
    for (auto g : a)   { idA  = g.first; valA  = g.second; ++foundA; }
    for (auto g : b)   { idB  = g.first; valB  = g.second; ++foundB; }
    for (auto g : *bn) { idBN = g.first; valBN = g.second; ++foundBN; }
    for (auto g : c)   { idC  = g.first; valC  = g.second; ++foundC; }

    EXPECT_EQ(1, foundA); EXPECT_EQ(real_idA, idA); EXPECT_EQ(real_valA, valA);
    EXPECT_EQ(1, foundB); EXPECT_EQ(real_idB, idB); EXPECT_EQ(real_valB, valB);
    EXPECT_EQ(1, foundBN); EXPECT_EQ(real_idB, idBN); EXPECT_EQ(real_valB, valBN);
    EXPECT_EQ(1, foundC); EXPECT_EQ(real_idC, idC); EXPECT_EQ(real_valC, valC);

    EXPECT_THROW(Bundle z(3, -1), Bundle::negativity_error);
}

#define GIMME_BUNDLES\
    BundleNegative a {{23,-4.5}, {45,100}, {678,0}, {2, -483.125}};\
    Bundle b {{44,0}, {55,12}, {100000000000,1e-10}};\
    Bundle c;\
    BundleNegative d {{1, 0}, {2, 0}, {3, 1}};\
    BundleNegative e {{1, 0}, {2, 0}, {3, 0}};\
    Bundle a2 {{23,4.5}, {45,100}, {678,0}, {2,483.125}};\
    BundleNegative b2 {{44,0}, {55,-12}, {100000000000,1e-10}};

TEST(Construction, InitLists) {
    GIMME_BUNDLES;

    eris_id_t idprodA = 1, idprodB = 1;
    double asum = 0, bsum = 0;
    int foundA = 0, foundB = 0;
    for (auto g : a) { idprodA *= g.first; asum += g.second; ++foundA; }
    for (auto g : b) { idprodB *= g.first; bsum += g.second; ++foundB; }

    EXPECT_EQ(4, foundA);
    EXPECT_EQ(3, foundB);

    EXPECT_EQ(1403460, idprodA);
    EXPECT_EQ(242000000000000, idprodB);

    EXPECT_EQ(-387.625, asum);
    EXPECT_EQ(12.0000000001, bsum);

    EXPECT_THROW(Bundle z({{3, -1}}), Bundle::negativity_error);
    EXPECT_THROW(Bundle z({{3, 1}, {4, -71}}), Bundle::negativity_error);
}

TEST(Construction, copy) {
    GIMME_BUNDLES;

    Bundle bcopy = b;
    bcopy.set(44, 3);
    EXPECT_EQ(3, bcopy[44]);
    EXPECT_EQ(0, b[44]);

    BundleNegative acopy = a;
    acopy.set(23, 12);
    EXPECT_EQ(12, acopy[23]);
    EXPECT_EQ(-4.5, a[23]);
}

TEST(Properties, empty) {
    GIMME_BUNDLES;

    EXPECT_FALSE(a.empty());
    EXPECT_FALSE(b.empty());
    EXPECT_TRUE(c.empty());
    EXPECT_FALSE(d.empty());
    EXPECT_FALSE(e.empty());

    d.remove(1);
    EXPECT_FALSE(d.empty());
    d.remove(2);
    EXPECT_FALSE(d.empty());
    d.remove(3);
    EXPECT_TRUE(d.empty());
    d.set(4, 0);
    EXPECT_FALSE(d.empty());
}
TEST(Properties, size) {
    GIMME_BUNDLES;

    EXPECT_EQ(4, a.size());
    EXPECT_EQ(3, b.size());
    EXPECT_EQ(0, c.size());
    EXPECT_EQ(3, d.size());
    EXPECT_EQ(3, e.size());

    d.remove(3);
    EXPECT_EQ(2, d.size());
    d.remove(2);
    EXPECT_EQ(1, d.size());
    d.remove(1);
    EXPECT_EQ(0, d.size());
    d.set(1, 0);
    EXPECT_EQ(1, d.size());
    d.set(1, 4);
    EXPECT_EQ(1, d.size());
}
TEST(Properties, count) {
    GIMME_BUNDLES;

    EXPECT_EQ(1, a.count(23));
    EXPECT_EQ(0, a.count(24));
    EXPECT_EQ(1, b.count(44));
    EXPECT_EQ(1, b.count(100000000000));
    b.remove(100000000000);
    EXPECT_EQ(0, b.count(100000000000));
    b.set(100000000000, 0);
    EXPECT_EQ(1, b.count(100000000000));
    b.set(100000000000, 124142424442999);
    EXPECT_EQ(1, b.count(100000000000));
}


TEST(Access, index) {
    GIMME_BUNDLES

    EXPECT_EQ(100, a[45]);
    EXPECT_EQ(0, a[678]);
    EXPECT_EQ(-483.125, a[2]);
    EXPECT_EQ(-4.5, a[23]);
    EXPECT_EQ(0, a[1]);
    EXPECT_EQ(0, a[24242424]);

    EXPECT_EQ(0, b[1]);
    EXPECT_EQ(0, b[2]);
    EXPECT_EQ(0.0000000001, b[100000000000]);
    EXPECT_EQ(100, a[45]);
}
TEST(Access, iterator) {
    GIMME_BUNDLES

    eris_id_t idprodA = 1, idprodB = 1;
    double asum = 0, bsum = 0;
    int foundA = 0, foundB = 0;
    for (auto g : a) { idprodA *= g.first; asum += g.second; ++foundA; }
    for (auto g : b) { idprodB *= g.first; bsum += g.second; ++foundB; }

    EXPECT_EQ(4, foundA);
    EXPECT_EQ(3, foundB);

    EXPECT_EQ(1403460, idprodA);
    EXPECT_EQ(242000000000000, idprodB);

    EXPECT_EQ(-387.625, asum);
    EXPECT_EQ(12.0000000001, bsum);

    idprodA = idprodB = 1;
    asum = bsum = 0;
    for (auto it = a.begin(); it != a.end(); it++) {
        idprodA *= it->first;
        asum += it->second;
    }
    for (auto it = b.begin(); it != b.end(); it++) {
        idprodB *= it->first;
        bsum += it->second;
    }

    EXPECT_EQ(1403460, idprodA);
    EXPECT_EQ(242000000000000, idprodB);

    EXPECT_EQ(-387.625, asum);
    EXPECT_EQ(12.0000000001, bsum);
}
TEST(Access, NoVivify) {
    GIMME_BUNDLES

    // Repeat the index tests, above
    EXPECT_EQ(100, a[45]); // real
    EXPECT_EQ(0, a[678]); // real
    EXPECT_EQ(-483.125, a[2]); // real
    EXPECT_EQ(0, a[1]); // FAKE
    EXPECT_EQ(-4.5, a[23]); // real
    EXPECT_EQ(0, a[24242424]); // FAKE

    EXPECT_EQ(0, b[1]);
    EXPECT_EQ(0, b[2]);
    EXPECT_EQ(0.0000000001, b[100000000000]);
    EXPECT_EQ(100, a[45]);

    EXPECT_EQ(0, a.count(1));
    EXPECT_EQ(0, a.count(24242424));
    EXPECT_EQ(1, a.count(23));

    EXPECT_EQ(0, b.count(1));
    EXPECT_EQ(0, b.count(2));
    EXPECT_EQ(1, b.count(100000000000));
    // Now check to make sure we don't find the accessed ones when we iterate
}

#define COMPARE_BUNDLES\
    Bundle bb[100];\
/* [0] through [5] are for bundle-bundle relation comparisons: */ \
    bb[0] = {{1,3}, {2,12}};\
    bb[1] = {{1,5},        {3,1}};\
    bb[2] = {{1,6}, {2,6}, {3,0.125}};\
    bb[3] = {{1,1}, {2,1}};\
    bb[4] = {{1,8}, {2,12}, {3,1}}; /* b0 + b1 */ \
    bb[5] = {{1,6}, {2,6}, {3,0.125}}; /* == b2 */ \
/* These are for bundle-constant comparisons: */ \
    bb[6] = {{1,6}, {2,6}};\
    bb[7] = {{1,6}, {2,6}, {3,0}};\
    bb[8] = {{1,1}, {2,3}, {3,1}, {4,0}};\
    bb[9] = {{1,4}, {3,1}, {4,4}};

#define E_T(a) EXPECT_TRUE(a)
#define E_F(a) EXPECT_FALSE(a)

TEST(Relations, BundleEqBundle) {
    COMPARE_BUNDLES;
    E_T(bb[0] == bb[0]); E_F(bb[0] == bb[1]); E_F(bb[0] == bb[2]); E_F(bb[0] == bb[3]); E_F(bb[0] == bb[4]); E_F(bb[0] == bb[5]);
    E_F(bb[1] == bb[0]); E_T(bb[1] == bb[1]); E_F(bb[1] == bb[2]); E_F(bb[1] == bb[3]); E_F(bb[1] == bb[4]); E_F(bb[1] == bb[5]);
    E_F(bb[2] == bb[0]); E_F(bb[2] == bb[1]); E_T(bb[2] == bb[2]); E_F(bb[2] == bb[3]); E_F(bb[2] == bb[4]); E_T(bb[2] == bb[5]);
    E_F(bb[3] == bb[0]); E_F(bb[3] == bb[1]); E_F(bb[3] == bb[2]); E_T(bb[3] == bb[3]); E_F(bb[3] == bb[4]); E_F(bb[3] == bb[5]);
    E_F(bb[4] == bb[0]); E_F(bb[4] == bb[1]); E_F(bb[4] == bb[2]); E_F(bb[4] == bb[3]); E_T(bb[4] == bb[4]); E_F(bb[4] == bb[5]);
    E_F(bb[5] == bb[0]); E_F(bb[5] == bb[1]); E_T(bb[5] == bb[2]); E_F(bb[5] == bb[3]); E_F(bb[5] == bb[4]); E_T(bb[5] == bb[5]);
}
TEST(Relations, BundleNEqBundle) {
    COMPARE_BUNDLES;
    E_F(bb[0] != bb[0]); E_T(bb[0] != bb[1]); E_T(bb[0] != bb[2]); E_T(bb[0] != bb[3]); E_T(bb[0] != bb[4]); E_T(bb[0] != bb[5]);
    E_T(bb[1] != bb[0]); E_F(bb[1] != bb[1]); E_T(bb[1] != bb[2]); E_T(bb[1] != bb[3]); E_T(bb[1] != bb[4]); E_T(bb[1] != bb[5]);
    E_T(bb[2] != bb[0]); E_T(bb[2] != bb[1]); E_F(bb[2] != bb[2]); E_T(bb[2] != bb[3]); E_T(bb[2] != bb[4]); E_F(bb[2] != bb[5]);
    E_T(bb[3] != bb[0]); E_T(bb[3] != bb[1]); E_T(bb[3] != bb[2]); E_F(bb[3] != bb[3]); E_T(bb[3] != bb[4]); E_T(bb[3] != bb[5]);
    E_T(bb[4] != bb[0]); E_T(bb[4] != bb[1]); E_T(bb[4] != bb[2]); E_T(bb[4] != bb[3]); E_F(bb[4] != bb[4]); E_T(bb[4] != bb[5]);
    E_T(bb[5] != bb[0]); E_T(bb[5] != bb[1]); E_F(bb[5] != bb[2]); E_T(bb[5] != bb[3]); E_T(bb[5] != bb[4]); E_F(bb[5] != bb[5]);
}
TEST(Relations, BundleGTBundle) {
    COMPARE_BUNDLES;
    E_F(bb[0] >  bb[0]); E_F(bb[0] >  bb[1]); E_F(bb[0] >  bb[2]); E_T(bb[0] >  bb[3]); E_F(bb[0] >  bb[4]); E_F(bb[0] >  bb[5]);
    E_F(bb[1] >  bb[0]); E_F(bb[1] >  bb[1]); E_F(bb[1] >  bb[2]); E_F(bb[1] >  bb[3]); E_F(bb[1] >  bb[4]); E_F(bb[1] >  bb[5]);
    E_F(bb[2] >  bb[0]); E_F(bb[2] >  bb[1]); E_F(bb[2] >  bb[2]); E_T(bb[2] >  bb[3]); E_F(bb[2] >  bb[4]); E_F(bb[2] >  bb[5]);
    E_F(bb[3] >  bb[0]); E_F(bb[3] >  bb[1]); E_F(bb[3] >  bb[2]); E_F(bb[3] >  bb[3]); E_F(bb[3] >  bb[4]); E_F(bb[3] >  bb[5]);
    E_F(bb[4] >  bb[0]); E_F(bb[4] >  bb[1]); E_T(bb[4] >  bb[2]); E_T(bb[4] >  bb[3]); E_F(bb[4] >  bb[4]); E_T(bb[4] >  bb[5]);
    E_F(bb[5] >  bb[0]); E_F(bb[5] >  bb[1]); E_F(bb[5] >  bb[2]); E_T(bb[5] >  bb[3]); E_F(bb[5] >  bb[4]); E_F(bb[5] >  bb[5]);
}
TEST(Relations, BundleGTEqBundle) {
    COMPARE_BUNDLES;
    E_T(bb[0] >= bb[0]); E_F(bb[0] >= bb[1]); E_F(bb[0] >= bb[2]); E_T(bb[0] >= bb[3]); E_F(bb[0] >= bb[4]); E_F(bb[0] >= bb[5]);
    E_F(bb[1] >= bb[0]); E_T(bb[1] >= bb[1]); E_F(bb[1] >= bb[2]); E_F(bb[1] >= bb[3]); E_F(bb[1] >= bb[4]); E_F(bb[1] >= bb[5]);
    E_F(bb[2] >= bb[0]); E_F(bb[2] >= bb[1]); E_T(bb[2] >= bb[2]); E_T(bb[2] >= bb[3]); E_F(bb[2] >= bb[4]); E_T(bb[2] >= bb[5]);
    E_F(bb[3] >= bb[0]); E_F(bb[3] >= bb[1]); E_F(bb[3] >= bb[2]); E_T(bb[3] >= bb[3]); E_F(bb[3] >= bb[4]); E_F(bb[3] >= bb[5]);
    E_T(bb[4] >= bb[0]); E_T(bb[4] >= bb[1]); E_T(bb[4] >= bb[2]); E_T(bb[4] >= bb[3]); E_T(bb[4] >= bb[4]); E_T(bb[4] >= bb[5]);
    E_F(bb[5] >= bb[0]); E_F(bb[5] >= bb[1]); E_T(bb[5] >= bb[2]); E_T(bb[5] >= bb[3]); E_F(bb[5] >= bb[4]); E_T(bb[5] >= bb[5]);
}
TEST(Relations, BundleLTBundle) {
    COMPARE_BUNDLES;
    E_F(bb[0] <  bb[0]); E_F(bb[0] <  bb[1]); E_F(bb[0] <  bb[2]); E_F(bb[0] <  bb[3]); E_F(bb[0] <  bb[4]); E_F(bb[0] <  bb[5]);
    E_F(bb[1] <  bb[0]); E_F(bb[1] <  bb[1]); E_F(bb[1] <  bb[2]); E_F(bb[1] <  bb[3]); E_F(bb[1] <  bb[4]); E_F(bb[1] <  bb[5]);
    E_F(bb[2] <  bb[0]); E_F(bb[2] <  bb[1]); E_F(bb[2] <  bb[2]); E_F(bb[2] <  bb[3]); E_T(bb[2] <  bb[4]); E_F(bb[2] <  bb[5]);
    E_T(bb[3] <  bb[0]); E_F(bb[3] <  bb[1]); E_T(bb[3] <  bb[2]); E_F(bb[3] <  bb[3]); E_T(bb[3] <  bb[4]); E_T(bb[3] <  bb[5]);
    E_F(bb[4] <  bb[0]); E_F(bb[4] <  bb[1]); E_F(bb[4] <  bb[2]); E_F(bb[4] <  bb[3]); E_F(bb[4] <  bb[4]); E_F(bb[4] <  bb[5]);
    E_F(bb[5] <  bb[0]); E_F(bb[5] <  bb[1]); E_F(bb[5] <  bb[2]); E_F(bb[5] <  bb[3]); E_T(bb[5] <  bb[4]); E_F(bb[5] <  bb[5]);
}
TEST(Relations, BundleLTEqBundle) {
    COMPARE_BUNDLES;
    E_T(bb[0] <= bb[0]); E_F(bb[0] <= bb[1]); E_F(bb[0] <= bb[2]); E_F(bb[0] <= bb[3]); E_T(bb[0] <= bb[4]); E_F(bb[0] <= bb[5]);
    E_F(bb[1] <= bb[0]); E_T(bb[1] <= bb[1]); E_F(bb[1] <= bb[2]); E_F(bb[1] <= bb[3]); E_T(bb[1] <= bb[4]); E_F(bb[1] <= bb[5]);
    E_F(bb[2] <= bb[0]); E_F(bb[2] <= bb[1]); E_T(bb[2] <= bb[2]); E_F(bb[2] <= bb[3]); E_T(bb[2] <= bb[4]); E_T(bb[2] <= bb[5]);
    E_T(bb[3] <= bb[0]); E_F(bb[3] <= bb[1]); E_T(bb[3] <= bb[2]); E_T(bb[3] <= bb[3]); E_T(bb[3] <= bb[4]); E_T(bb[3] <= bb[5]);
    E_F(bb[4] <= bb[0]); E_F(bb[4] <= bb[1]); E_F(bb[4] <= bb[2]); E_F(bb[4] <= bb[3]); E_T(bb[4] <= bb[4]); E_F(bb[4] <= bb[5]);
    E_F(bb[5] <= bb[0]); E_F(bb[5] <= bb[1]); E_T(bb[5] <= bb[2]); E_F(bb[5] <= bb[3]); E_T(bb[5] <= bb[4]); E_T(bb[5] <= bb[5]);
}

TEST(Relations, BundleEqConstant) {
    COMPARE_BUNDLES;
    E_F(bb[0] == 3);
    E_F(bb[2] == 6);
    E_F(bb[5] == 6);
    E_T(bb[6] == 6);
    E_F(bb[7] == 6);
}
TEST(Relations, BundleNEqConstant) {
    COMPARE_BUNDLES;
    E_T(bb[0] != 3);
    E_T(bb[2] != 6);
    E_T(bb[5] != 6);
    E_F(bb[6] != 6);
    E_T(bb[7] != 6);
}
TEST(Relations, BundleGTConstant) {
    COMPARE_BUNDLES;
    E_F(bb[0] > 3);
    E_T(bb[0] > 2.999);
    E_F(bb[2] > 6);
    E_F(bb[5] > 5);
    E_T(bb[6] > 5);
    E_F(bb[6] > 6);
    E_F(bb[7] > 5);
    E_F(bb[7] > 6);
}
TEST(Relations, BundleGTEqConstant) {
    COMPARE_BUNDLES;
    E_T(bb[0] >= 3);
    E_T(bb[0] >= 2.999);
    E_F(bb[0] >= 3.0001);
    E_F(bb[2] >= 6);
    E_F(bb[5] >= 5);
    E_T(bb[6] >= 5);
    E_T(bb[6] >= 6);
    E_F(bb[6] >= 6.0000001);
    E_F(bb[7] >= 5);
    E_F(bb[7] >= 6);
    E_T(bb[7] >= 0);
    E_T(bb[7] >= -1);
}
TEST(Relations, BundleLTConstant) {
    COMPARE_BUNDLES;
    E_F(bb[0] < 3);
    E_F(bb[0] < 2.999);
    E_F(bb[0] < 3.0001);
    E_F(bb[2] < 6);
    E_T(bb[2] < 6.001);
    E_F(bb[5] < 5);
    E_F(bb[6] < 5);
    E_F(bb[6] < 6);
    E_T(bb[6] < 6.0000001);
    E_F(bb[7] < 5);
    E_F(bb[7] < 6);
    E_T(bb[7] < 6.001);
    E_F(bb[7] < 0);
    E_F(bb[7] < -1);
}
TEST(Relations, BundleLTEqConstant) {
    COMPARE_BUNDLES;
    E_F(bb[0] <= 3);
    E_F(bb[0] <= 2.999);
    E_F(bb[0] <= 3.0001);
    E_T(bb[2] <= 6);
    E_T(bb[2] <= 6.001);
    E_F(bb[5] <= 5);
    E_F(bb[6] <= 5);
    E_T(bb[6] <= 6);
    E_T(bb[6] <= 6.0000001);
    E_F(bb[7] <= 5);
    E_T(bb[7] <= 6);
    E_T(bb[7] <= 6.001);
    E_F(bb[7] <= 0);
    E_F(bb[7] <= -1);
}

TEST(Relations, ConstantEqBundle) {
    COMPARE_BUNDLES;
    E_F(3 == bb[0]);
    E_F(6 == bb[2]);
    E_F(6 == bb[5]);
    E_T(6 == bb[6]);
    E_F(6 == bb[7]);
}
TEST(Relations, ConstantNEqBundle) {
    COMPARE_BUNDLES;
    E_T(3 != bb[0]);
    E_T(6 != bb[2]);
    E_T(6 != bb[5]);
    E_F(6 != bb[6]);
    E_T(6 != bb[7]);
}
TEST(Relations, ConstantLTBundle) {
    COMPARE_BUNDLES;
    E_F(3 < bb[0]);
    E_T(2.999 < bb[0]);
    E_F(6 < bb[2]);
    E_F(5 < bb[5]);
    E_T(5 < bb[6]);
    E_F(6 < bb[6]);
    E_F(5 < bb[7]);
    E_F(6 < bb[7]);
}
TEST(Relations, ConstantLTEqBundle) {
    COMPARE_BUNDLES;
    E_T(3 <= bb[0]);
    E_T(2.999 <= bb[0]);
    E_F(3.0001 <= bb[0]);
    E_F(6 <= bb[2]);
    E_F(5 <= bb[5]);
    E_T(5 <= bb[6]);
    E_T(6 <= bb[6]);
    E_F(6.0000001 <= bb[6]);
    E_F(5 <= bb[7]);
    E_F(6 <= bb[7]);
    E_T(0 <= bb[7]);
    E_T(-1 <= bb[7]);
}
TEST(Relations, ConstantGTBundle) {
    COMPARE_BUNDLES;
    E_F(3 > bb[0]);
    E_F(2.999 > bb[0]);
    E_F(3.0001 > bb[0]);
    E_F(6 > bb[2]);
    E_T(6.001 > bb[2]);
    E_F(5 > bb[5]);
    E_F(5 > bb[6]);
    E_F(6 > bb[6]);
    E_T(6.0000001 > bb[6]);
    E_F(5 > bb[7]);
    E_F(6 > bb[7]);
    E_T(6.001 > bb[7]);
    E_F(0 > bb[7]);
    E_F(-1 > bb[7]);
}
TEST(Relations, ConstantGTEqBundle) {
    COMPARE_BUNDLES;
    E_F(3 >= bb[0]);
    E_F(2.999 >= bb[0]);
    E_F(3.0001 >= bb[0]);
    E_T(6 >= bb[2]);
    E_T(6.001 >= bb[2]);
    E_F(5 >= bb[5]);
    E_F(5 >= bb[6]);
    E_T(6 >= bb[6]);
    E_T(6.0000001 >= bb[6]);
    E_F(5 >= bb[7]);
    E_T(6 >= bb[7]);
    E_T(6.001 >= bb[7]);
    E_F(0 >= bb[7]);
    E_F(-1 >= bb[7]);
}

TEST(Modification, setSingle) {
    GIMME_BUNDLES;

    a.set(23, 0);
    EXPECT_EQ(4, a.size());
    EXPECT_EQ(0, a[23]);
    EXPECT_EQ(100, a[45]);
    EXPECT_EQ(0, a[678]);
    EXPECT_EQ(-483.125, a[2]);
    EXPECT_EQ(0, 0);
    a.set(3, 1);
    EXPECT_EQ(5, a.size());
    EXPECT_EQ(0, a[23]);
    EXPECT_EQ(100, a[45]);
    EXPECT_EQ(0, a[678]);
    EXPECT_EQ(-483.125, a[2]);
    EXPECT_EQ(0, a[0]);
    EXPECT_EQ(1, a[3]);

    a2.set(23, 0);
    EXPECT_EQ(4, a2.size());
    EXPECT_EQ(0, a2[23]);
    EXPECT_EQ(100, a2[45]);
    EXPECT_EQ(0, a2[678]);
    EXPECT_EQ(483.125, a2[2]);
    EXPECT_EQ(0, 0);
    a2.set(3, 11);
    EXPECT_EQ(5, a2.size());
    EXPECT_EQ(0, a2[23]);
    EXPECT_EQ(100, a2[45]);
    EXPECT_EQ(0, a2[678]);
    EXPECT_EQ(483.125, a2[2]);
    EXPECT_EQ(0, a2[0]);
    EXPECT_EQ(11, a2[3]);
}
TEST(Modification, erase) {
    GIMME_BUNDLES;

    a.erase(56);
    EXPECT_EQ(4, a.size());
    a.erase(23);
    EXPECT_EQ(3, a.size());
    a.erase(23);
    EXPECT_EQ(3, a.size());
    a.erase(45);
    EXPECT_EQ(2, a.size());
    a.erase(678);
    EXPECT_EQ(1, a.size());
    a.erase(2);
    EXPECT_EQ(0, a.size());
    EXPECT_TRUE(a.empty());

    a2.erase(56);
    EXPECT_EQ(4, a2.size());
    a2.erase(23);
    EXPECT_EQ(3, a2.size());
    a2.erase(23);
    EXPECT_EQ(3, a2.size());
    a2.erase(45);
    EXPECT_EQ(2, a2.size());
    a2.erase(678);
    EXPECT_EQ(1, a2.size());
    a2.erase(2);
    EXPECT_EQ(0, a2.size());
    EXPECT_TRUE(a2.empty());

}
TEST(Modification, remove) {
    GIMME_BUNDLES;

    EXPECT_EQ(a.remove(56), 0);
    EXPECT_EQ(4, a.size());
    EXPECT_EQ(a.remove(23), -4.5);
    EXPECT_EQ(3, a.size());
    EXPECT_EQ(a.remove(23), 0);
    EXPECT_EQ(3, a.size());
    EXPECT_EQ(a.remove(45), 100);
    EXPECT_EQ(2, a.size());
    EXPECT_EQ(a.remove(678), 0);
    EXPECT_EQ(1, a.size());
    EXPECT_EQ(a.remove(2), -483.125);
    EXPECT_EQ(0, a.size());
    EXPECT_TRUE(a.empty());

    EXPECT_EQ(a2.remove(56), 0);
    EXPECT_EQ(4, a2.size());
    EXPECT_EQ(a2.remove(23), 4.5);
    EXPECT_EQ(3, a2.size());
    EXPECT_EQ(a2.remove(23), 0);
    EXPECT_EQ(3, a2.size());
    EXPECT_EQ(a2.remove(45), 100);
    EXPECT_EQ(2, a2.size());
    EXPECT_EQ(a2.remove(678), 0);
    EXPECT_EQ(1, a2.size());
    EXPECT_EQ(a2.remove(2), 483.125);
    EXPECT_EQ(0, a2.size());
    EXPECT_TRUE(a2.empty());
}
TEST(Modification, clearZeros) {
    GIMME_BUNDLES;

    a.clearZeros();
    EXPECT_EQ(a.size(), 3);
    a2.clearZeros();
    EXPECT_EQ(a2.size(), 3);
    b.clearZeros();
    EXPECT_EQ(b.size(), 2);
    b2.clearZeros();
    EXPECT_EQ(b2.size(), 2);
    c.clearZeros();
    EXPECT_EQ(c.size(), 0);
    d.clearZeros();
    EXPECT_EQ(d.size(), 1);
    e.clearZeros();
    EXPECT_EQ(e.size(), 0);

    Bundle zeros {{1,0}, {2,0}, {3,0}, {4,0}};
    zeros.clearZeros();
    EXPECT_EQ(e.size(), 0);
}

TEST(Algebra, Addition) {
    COMPARE_BUNDLES;

    // Make sure addition doesn't affect its operands
    Bundle temp = bb[0] + bb[1];
    EXPECT_EQ(Bundle({{1,3},{2,12}}), bb[0]);
    EXPECT_EQ(Bundle({{1,5},{3,1}}), bb[1]);

    EXPECT_EQ(bb[4], bb[0] + bb[1]);
    EXPECT_EQ(bb[2], bb[7] + Bundle({{3,0.125}}));
    EXPECT_EQ(Bundle({{1,11}, {2,6}, {3,1}}), bb[6] + bb[1]);

    EXPECT_EQ(BundleNegative({{2,3}, {4,4}}),
            BundleNegative({{1,-1}, {2,2}, {3,0}, {4,4}}) + BundleNegative({{1, 1}, {2,1}, {3,0}, {4,0}}));
}
TEST(Algebra, Subtraction) {
    COMPARE_BUNDLES;

    // Make sure subtraction doesn't affect its operands
    Bundle temp = bb[4] - bb[1];
    EXPECT_EQ(Bundle({{1,5},{3,1}}), bb[1]);
    EXPECT_EQ(Bundle({{1,8},{2,12}, {3,1}}), bb[4]);

    EXPECT_EQ(bb[1], bb[4] - bb[0]);
    EXPECT_EQ(Bundle({{3,0.125}}), bb[2] - bb[7]);
    EXPECT_EQ(bb[6], Bundle({{1,11}, {2,6}, {3,1}}) - bb[1]);
    EXPECT_EQ(0, bb[6] - bb[7]);

    EXPECT_THROW(bb[4] - Bundle({{4, 0.001}}), Bundle::negativity_error);
    EXPECT_NO_THROW(bb[8] - Bundle({{1, 1}, {2, 3}, {3, 1}, {4,0}, {5,0}}));
    EXPECT_THROW(bb[8] - Bundle({{1, 1}, {2, 3}, {3, 1.001}}), Bundle::negativity_error);

    EXPECT_EQ(BundleNegative({{1,-2}, {2,1}, {4,4}}),
            BundleNegative({{1,-1}, {2,2}, {3,0}, {4,4}}) - BundleNegative({{1, 1}, {2,1}, {3,0}, {4,0}}));
}
TEST(Algebra, UnaryMinus) {
    COMPARE_BUNDLES;

    // Make sure negation doesn't affect its argument:
    auto temp = -bb[4];
    EXPECT_EQ(Bundle({{1,8},{2,12}, {3,1}}), bb[4]);

    EXPECT_EQ(BundleNegative({{1,-6}, {2,-6}}), -bb[6]);

    EXPECT_EQ(BundleNegative({{1,1}, {2,-2}, {4,-4}}), -BundleNegative({{1,-1}, {2,2}, {3,0}, {4,4}}));
}
TEST(Algebra, BundleTimesConstant) {
    BundleNegative bn {{1,14}, {2,-3}, {3,0}};
    Bundle bp {{1,14}, {2,3}, {3,0}};

    // Make sure * doesn't affect argument
    auto temp = bn * 2;
    EXPECT_EQ(BundleNegative({{1,14},{2,-3}}), bn);

    EXPECT_EQ(BundleNegative({{1,-7}, {2,1.5}}), bn*-0.5);
    EXPECT_EQ(Bundle({{1,7}, {2,1.5}}), bp*0.5);

    EXPECT_THROW(bp * -3, Bundle::negativity_error);
}
TEST(Algebra, ConstantTimesBundle) {
    BundleNegative bn {{1,14}, {2,-3}, {3,0}};
    Bundle bp {{1,14}, {2,3}, {3,0}};

    // Make sure * doesn't affect argument
    auto temp = 2 * bn;
    EXPECT_EQ(BundleNegative({{1,14},{2,-3}}), bn);

    EXPECT_EQ(BundleNegative({{1,-7}, {2,1.5}}), -0.5*bn);
    EXPECT_EQ(Bundle({{1,7}, {2,1.5}}), 0.5*bp);
    
    EXPECT_THROW(-3 * bp, Bundle::negativity_error);
}
TEST(Algebra, BundleDividedbyConstant) {
    BundleNegative bn {{1,14}, {2,-3}, {3,0}};
    Bundle bp {{1,14}, {2,3}, {3,0}};

    // Make sure / doesn't affect argument
    auto temp = bn / 2;
    EXPECT_TRUE(bn == BundleNegative({{1,14},{2,-3}}));

    EXPECT_EQ(BundleNegative({{1,-7}, {2,1.5}}), bn/-2);
    EXPECT_EQ(Bundle({{1,7}, {2,1.5}}), bp/2);

    EXPECT_THROW(bp / -0.0001, Bundle::negativity_error);
}

TEST(AlgebraicModifiers, PlusEqualsBundle) {
    GIMME_BUNDLES;

    e += a;
    EXPECT_EQ(BundleNegative({{23,-4.5}, {45,100}, {2, -483.125}}), e);

    c += a2;
    EXPECT_EQ(Bundle({{23,4.5}, {45,100}, {2,483.125}}), c);

    a += d;
    a += a2;
    EXPECT_EQ(Bundle({{45,200}, {3,1}}), a);

    a2 += b;
    a2 += c; // == a2, from above
    a2 += a2;
    EXPECT_EQ(BundleNegative({{55,24}, {100000000000,0.0000000002}, {23,18}, {45,400}, {2,1932.5}}), a2);

    EXPECT_THROW(c += BundleNegative({{333, -0.125}}), Bundle::negativity_error);
    BundleNegative *cn = dynamic_cast<BundleNegative *>(&c);
    EXPECT_THROW(*cn += BundleNegative({{333, -0.125}}), Bundle::negativity_error);

    // Slightly more complicated: let's try adding  a negative quantity to the returned value of a +=
    EXPECT_THROW((c += Bundle(1,0)) += BundleNegative({{333, -0.125}}), Bundle::negativity_error);
}
TEST(AlgebraicModifiers, MinusEqualsBundle) {
    GIMME_BUNDLES;

    e -= a;
    EXPECT_EQ(BundleNegative({{23,4.5}, {45,-100}, {2, 483.125}}), e);

    c -= -a2;
    EXPECT_EQ(Bundle({{23,4.5}, {45,100}, {2,483.125}}), c);

    a -= d;
    a -= a2;
    EXPECT_EQ(BundleNegative({{23,-9}, {45,0}, {2,-966.25}, {3,-1}}), a);

    a2 -= Bundle {{23,4.4375}, {45,100}, {678,0}, {88,0}};
    EXPECT_EQ(Bundle({{23,0.0625}, {2,483.125}}), a2);

    EXPECT_THROW(c -= Bundle({{333, 0.125}}), Bundle::negativity_error);
    BundleNegative *cn = dynamic_cast<BundleNegative *>(&c);
    EXPECT_THROW(*cn -= Bundle({{333, 0.125}}), Bundle::negativity_error);
}
TEST(AlgebraicModifiers, TimesEqualConstant) {
    GIMME_BUNDLES;

    a2 *= 2;
    b2 *= -0.5;

    EXPECT_EQ(Bundle({{23,9}, {45,200}, {2,966.25}}), a2);
    EXPECT_EQ(BundleNegative({{55,6}, {100000000000,-5e-11}}), b2);

    EXPECT_THROW(b *= -1, Bundle::negativity_error);
    BundleNegative *a2n = dynamic_cast<BundleNegative *>(&a2);
    EXPECT_THROW(*a2n *= -3, Bundle::negativity_error);
}
TEST(AlgebraicModifiers, DivEqualConstant) {
    GIMME_BUNDLES;

    a2 /= 0.5;
    b2 /= -2;

    EXPECT_EQ(Bundle({{23,9}, {45,200}, {2,966.25}}), a2);
    EXPECT_EQ(BundleNegative({{55,6}, {100000000000,-5e-11}}), b2);

    EXPECT_THROW(b /= -1, Bundle::negativity_error);
    BundleNegative *a2n = dynamic_cast<BundleNegative *>(&a2);
    EXPECT_THROW(*a2n /= -3, Bundle::negativity_error);
}
TEST(AlgebraicModifiers, TransferApprox) {
    Bundle a {{1,  999}, {2,  9999}, {3, 100000}};
    Bundle c {{1, 5000}, {2, 40000}};

    // goods 1 and 2 should trigger the epsilon-rounding (for epsilon ~ 1e-3), 3 should not:
    BundleNegative transfer {{1, 1000}, {2,  9998}, {3, 95000}};

    Bundle aa = a;
    Bundle cc = c;
    aa.transferApprox(transfer, cc, 1.5e-3);
    EXPECT_EQ(Bundle({{1, 0}, {2, 0}, {3, 5000}}), aa);
    EXPECT_EQ(Bundle({{1, 5999}, {2,49999}, {3,95000}}), cc);

    // Try the same thing with a negative transfer
    Bundle ar = a;
    Bundle cr = c;
    cr.transferApprox(-transfer, ar, 1.5e-3);
    EXPECT_EQ(Bundle({{1, 0}, {2, 0}, {3, 5000}}), ar);
    EXPECT_EQ(Bundle({{1, 5999}, {2,49999}, {3,95000}}), cr);

    // Now test out the destination epsilon-rounding (which applies when the destination ends up
    // close to 0, which can only happen when the destination starts negative).
    BundleNegative an = -a;
    BundleNegative cn = c;
    cn.transferApprox(transfer, an, 1.5e-3);
    EXPECT_EQ(BundleNegative({{1, 0}, {2, 0}, {3, -5000}}), an);
    EXPECT_EQ(BundleNegative({{1, 4001}, {2,30001}, {3,-95000}}), cn);

    // Test both at once
    BundleNegative ab {{1,  999}, {2, -9999}, {3, 100000}, {4, 500}};
    BundleNegative cb {{1, 5000}, {2, 40000},              {4, 500.5}};

    cb.transferApprox(BundleNegative({{1,-1000}, {2, 10000}, {3, -95001}, {4,500}}), ab, 1.5e-3);

    EXPECT_EQ(Bundle({{1, 0}, {2, 0}, {3, 4999}, {4, 1000.5}}), ab);
    EXPECT_EQ(Bundle({{1, 5999}, {2, 30001}, {3, 95001}, {4, 0}}), cb);
}

TEST(AdvancedAlgebra, BundleDividedbyBundle) {
    COMPARE_BUNDLES;

    EXPECT_EQ(numeric_limits<double>::infinity(), bb[5] / bb[6]);
    EXPECT_EQ(1, bb[6] / bb[5]);
    EXPECT_EQ(numeric_limits<double>::infinity(), bb[4] / bb[0]);
    EXPECT_EQ(1, bb[0] / bb[4]);
    EXPECT_EQ(numeric_limits<double>::infinity(), bb[4] / bb[1]);
    EXPECT_EQ(1, bb[1] / bb[4]);
    EXPECT_EQ(8, bb[4] / bb[5]);
    EXPECT_EQ(0.75, bb[5] / bb[4]);
    EXPECT_EQ(1, bb[6] / bb[7]);
    EXPECT_EQ(1, bb[7] / bb[6]);

    EXPECT_TRUE(isnan((Bundle {}) / (Bundle {})));
}
TEST(AdvancedAlgebra, BundleModulusBundle) {
    COMPARE_BUNDLES;

    EXPECT_THROW(bb[5] % bb[6], Bundle::negativity_error);
    EXPECT_EQ(Bundle({{3,0.125}}), bb[6] % bb[5]);
    EXPECT_THROW(bb[4] % bb[0], Bundle::negativity_error);
    EXPECT_EQ(Bundle({{1,5}, {3,1}}), bb[0] % bb[4]);
    EXPECT_THROW(bb[4] % bb[1], Bundle::negativity_error);
    EXPECT_EQ(Bundle({{1,3}, {2,12}}), bb[1] % bb[4]);
    EXPECT_EQ(Bundle({{1,40}, {2,36}}), bb[4] % bb[5]);
    EXPECT_EQ(Bundle({{2,3}, {3,0.625}}), bb[5] % bb[4]);
    EXPECT_EQ(0, bb[6] % bb[7]);
    EXPECT_EQ(0, bb[7] % bb[6]);
}
TEST(AdvancedAlgebra, multiples) {
    Bundle a  {{1,100}, {2,10}};
    Bundle b  {         {2,1}};
    Bundle b0 {{1,0},   {2,1}};
    Bundle c  {{1,5}};
    Bundle c0 {{1,5},   {2,0}};
    Bundle z;
    Bundle z0 {{1,0},   {2,0}};

    EXPECT_EQ(numeric_limits<double>::infinity(), a/b);
    EXPECT_EQ(numeric_limits<double>::infinity(), a/b0);
    EXPECT_EQ(0.1, b/a);
    EXPECT_EQ(0.1, b0/a);
    EXPECT_EQ(10, a.multiples(b));
    EXPECT_EQ(10, a.multiples(b0));
    EXPECT_EQ(0, b.multiples(a));
    EXPECT_EQ(0, b0.multiples(a));

    EXPECT_EQ(numeric_limits<double>::infinity(), a/c);
    EXPECT_EQ(numeric_limits<double>::infinity(), a/c0);
    EXPECT_EQ(0.05, c/a);
    EXPECT_EQ(0.05, c0/a);
    EXPECT_EQ(20, a.multiples(c));
    EXPECT_EQ(20, a.multiples(c0));
    EXPECT_EQ(0, c.multiples(a));
    EXPECT_EQ(0, c0.multiples(a));

    EXPECT_EQ(numeric_limits<double>::infinity(), a/z);
    EXPECT_EQ(numeric_limits<double>::infinity(), a/z0);
    EXPECT_EQ(0, z/a);
    EXPECT_EQ(0, z0/a);
    EXPECT_EQ(0, z.multiples(a));
    EXPECT_EQ(0, z0.multiples(a));
    EXPECT_EQ(numeric_limits<double>::infinity(), a.multiples(z));
    EXPECT_EQ(numeric_limits<double>::infinity(), a.multiples(z0));

    EXPECT_TRUE(isnan(z / z));
    EXPECT_TRUE(isnan(z0 / z));
    EXPECT_TRUE(isnan(z / z0));
    EXPECT_TRUE(isnan(z0 / z0));
    EXPECT_TRUE(isnan(z.multiples(z)));
    EXPECT_TRUE(isnan(z0.multiples(z)));
    EXPECT_TRUE(isnan(z.multiples(z0)));
    EXPECT_TRUE(isnan(z0.multiples(z0)));
}

TEST(AdvancedAlgebra, covers) {
    COMPARE_BUNDLES;

    EXPECT_TRUE (bb[5].covers(bb[6]));
    EXPECT_FALSE(bb[6].covers(bb[5]));
    EXPECT_TRUE (bb[4].covers(bb[0]));
    EXPECT_FALSE(bb[0].covers(bb[4]));
    EXPECT_TRUE (bb[4].covers(bb[1]));
    EXPECT_FALSE(bb[1].covers(bb[4]));
    EXPECT_TRUE (bb[4].covers(bb[5]));
    EXPECT_TRUE (bb[5].covers(bb[4]));
    EXPECT_TRUE (bb[6].covers(bb[7]));
    EXPECT_TRUE (bb[7].covers(bb[6]));
}
TEST(AdvancedAlgebra, common) {
    COMPARE_BUNDLES;

    EXPECT_EQ(Bundle({{1,3}}), Bundle::common(bb[0], bb[1]));
    EXPECT_EQ(Bundle({{1,3}}), Bundle::common(bb[1], bb[0]));
    EXPECT_EQ(Bundle({{1,3}, {2,6}}), Bundle::common(bb[0], bb[2]));
    EXPECT_EQ(Bundle({{1,1}, {2,3}}), Bundle::common(bb[0], bb[8]));
    EXPECT_EQ(bb[8], Bundle::common(bb[8], bb[8]));
    EXPECT_EQ(4, Bundle::common(bb[8], bb[8]).size());

    Bundle c89 = Bundle::common(bb[8], bb[9]);
    Bundle c98 = Bundle::common(bb[9], bb[8]);
    EXPECT_EQ(Bundle({{1,1}, {3,1}, {4,0}}), c89);
    EXPECT_EQ(Bundle({{1,1}, {3,1}, {4,0}}), c98);
    EXPECT_EQ(1, c89.count(4));
    EXPECT_EQ(1, c98.count(4));
    EXPECT_EQ(3, c89.size());
    EXPECT_EQ(3, c98.size());

    Bundle cempty = Bundle::common(bb[8], Bundle {});
    EXPECT_EQ(0, cempty);
    EXPECT_EQ(0, cempty.size());
}
TEST(AdvancedAlgebra, reduce) {
    COMPARE_BUNDLES;

    Bundle r01 = Bundle::reduce(bb[0], bb[1]);
    EXPECT_EQ(Bundle({{1,3}}), r01);
    EXPECT_EQ(1, r01.size());
    EXPECT_EQ(Bundle({{2,12}}), bb[0]);
    EXPECT_EQ(2, bb[0].size());
    EXPECT_EQ(Bundle({{1,2}, {3,1}}), bb[1]);
    EXPECT_EQ(2, bb[1].size());

    Bundle r02 = Bundle::reduce(bb[0], bb[2]); // NB: bb[0] reduced above
    EXPECT_EQ(Bundle({{2,6}}), r02);
    EXPECT_EQ(2, r02.size());
    EXPECT_EQ(Bundle({{2,6}}), bb[0]);
    EXPECT_EQ(2, bb[0].size());
    EXPECT_EQ(Bundle({{1,6}, {3,0.125}}), bb[2]);
    EXPECT_EQ(3, bb[2].size());

    Bundle r89 = Bundle::reduce(bb[8], bb[9]);
    EXPECT_EQ(Bundle({{1,1}, {3,1}}), r89);
    EXPECT_EQ(3, r89.size());
    EXPECT_EQ(Bundle({{2,3}}), bb[8]);
    EXPECT_EQ(4, bb[8].size());
    EXPECT_EQ(Bundle({{1,3}, {4,4}}), bb[9]);
    EXPECT_EQ(3, bb[9].size());

    EXPECT_THROW(Bundle::reduce(bb[6], bb[6]), std::invalid_argument);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

