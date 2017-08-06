#include <eris/algorithms.hpp>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <sstream>
#ifdef SEARCH_F128
extern "C" {
#include <quadmath.h>
}
#endif

template <typename T> std::string tostr(const T &v) {
    std::ostringstream s;
    s << std::setprecision(20) << std::boolalpha << v;
    return s.str();
}

#ifdef SEARCH_F128
template <> std::string tostr(const __float128 &v) {
    char buf[128];
    quadmath_snprintf(buf, sizeof buf, "%.40Qf", v);
    return std::string(buf);
}
#endif

template <typename R> void print(const R &r) {
    std::cout << "arg=" << tostr(r.arg) << ", val=" << tostr(r.value) << ", inside=" << r.inside << ", iterations=" << r.iterations << "\n";
}

#ifdef SEARCH_F128
template <> constexpr __float128 eris::phi<__float128> =
    1.61803398874989484820458683436563811Q;
#endif

int main() {
#ifndef SEARCH_F128
    using FloatType = double;
#else
    using FloatType = __float128;
#endif

    print(eris::single_peak_search<FloatType>(
            [](FloatType x) { return 12 - x*x + 3*x; },
            0, 100, 0));

}
