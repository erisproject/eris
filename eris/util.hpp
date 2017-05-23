#pragma once
#include <eris/types.hpp>
#include <utility>

namespace eris {

/** Wrapper class around a pair of iterators that converts the pair into a range, so that a
 * for-range statement can be used.  The primary target of this is multimap's equal_range method,
 * which returns just such a pair.  This class is typically invoked via the range() function.
 */
template <class Iter>
class range_ final : public std::pair<Iter, Iter> {
    public:
        /// Builds an iteratable range from a start and end iterator
        range_(std::pair<Iter, Iter> const &pair) : std::pair<Iter, Iter>(pair) {}
        /// Returns the beginning of the range
        Iter begin() const { return this->first;  }
        /// Returns the end of the range
        Iter end()   const { return this->second; }
};
/** Takes a std::pair of iterators that represents a range, and returns an iterable object for that
 * range.  This is intended to allow for range-based for loops for methods that return a pair of
 * iterators representing a range, such as multimap's equal_range() method.
 *
 * Example:
 *
 *     for (auto &whatever : eris::range(mmap.equal_range(key))) {
 *         ...
 *     }
 */
template <class Iter>
range_<Iter> range(std::pair<Iter, Iter> const &pair) {
    return range_<Iter>(pair);
}


#if __cplusplus > 201103L
/** C++14-compatible index_sequence; under C++14, this is just a typedef for std::index_sequence;
 * under C++11, we provide a compatible implementation.
 */
template <size_t... Ints> using index_sequence = std::index_sequence<Ints...>;
/** C++14 index sequence helper; under C++14, this is just a typedef for std::make_index_sequence;
 * under C++11, we provide a compatible implementation.
 */
template <size_t N> using make_index_sequence = std::make_index_sequence<N>;
#else
/** C++14-compatible index_sequence; under C++14, this is just a typedef for std::index_sequence;
 * under C++11, we provide a compatible implementation.
 */
template <size_t... Ints> struct index_sequence {
    /// Equal to size_t
    using value_type = size_t;
    /// Returns the number of elements in the sequence
    static constexpr size_t size() { return sizeof...(Ints); }
};
/// \internal
template <size_t N, size_t... Tail> struct _make_index_sequence : _make_index_sequence<N-1, N-1, Tail...> {};
/// \internal
template <size_t... Sequence> struct _make_index_sequence<0, Sequence...> { using type = index_sequence<Sequence...>; };
/** C++14 index sequence helper; under C++14, this is just a typedef for std::make_index_sequence;
 * under C++11, we provide a compatible implementation.
 */
template <size_t N> using make_index_sequence = typename _make_index_sequence<N>::type;
#endif

}
