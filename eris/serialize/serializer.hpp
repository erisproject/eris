#pragma once
#include <cstdint>
#include <iostream>
#include <boost/detail/endian.hpp>
#include <array>
#include <eris/util.hpp>

namespace eris { namespace serialize {

/** Conversion template class for serializing or deserializing a value to or from an output or input
 * stream.
 *
 * Every serializer specialization must inherit from serializer_base or const_serializer_base and
 * implement the store_to (both) and load_from (serializer_base only) methods.
 *
 * The default implementation supports arithmetic types (that is, integer and floating point types).
 * Note, however, that when using integer types, one should stick to the types provided by
 * '<cstdint>' such as `uint64_t`, `int16_t`, etc., because other integer types may mean different
 * things on different systems (e.g. `long` could be 32 or 64 bits).
 *
 * Classes can make themself serializable by providing a serializer specialization.  The default
 * implementation is designed to be endianness-agnostic; files will most likely not be portable
 * across architectures with more fundamental differences (e.g. different floating point
 * representations).
 *
 * If the serialized type has a constant size, it should be exposed by declaring a static `size_t
 * size` member indicating the size; this will allow it to be used in places such as
 * Serializer::addHeaderField, which only allow constant-sized serialization values.  To explicitly
 * indicate a non-constant size, a `size` field can be declared with a value of 0.
 */
template <typename T, typename Sfinae = void>
class serializer;

/** Base class for read/write serializable types.  The base class converts << and >> calls on
 * std::ostreams and std::istreams into calls to the abstract virtual methods load_from() and
 * store_to().
 *
 * It also provides convenience methods for writing or reading serialized values of arbitrary types.
 *
 * For const types, inherit from const_serializer_base instead.
 */
class serializer_base {
public:
    /// Virtual destructor.
    virtual ~serializer_base() = default;

    /// The default size, 0, which indicates non-constant size.  Constant-size serializers should
    /// mask this with their own non-zero value.
    static constexpr size_t size = 0;

    /// Replaces the current value with a value read from the given input stream.
    virtual void load_from(std::istream &is) = 0;

    /// Writes the current value to the given output stream.
    virtual void store_to(std::ostream &os) const = 0;

    /** Writes a serialization of val to output stream os.  T must have a serializer<T>
     * implementation.
     */
    template <typename T>
    void write(std::ostream &os, T &val) {
        os << serializer<T>(val);
    }

    /** Reads and returns a serialization of type T from input stream is.  T must be default
     * constructible and must have a serializer<T> implementation.
     */
    template <typename T>
    T read(std::istream &is) {
        T val;
        is >> serializer<T>(val);
        return val;
    }
};

/** Convenience base class for serialization of const values; this extends serializer_base by
 * implementing a load_from() method that raises an exception if called.
 *
 * It is not required to use this class for const types: this class is simply an easy way to avoid
 * needing to provide a load_from method, since load_from isn't supported on const types.
 */
class const_serializer_base : public serializer_base {
public:
    /// \throws std::logic_error - const references can't be 
    virtual void load_from(std::istream&) override { throw std::logic_error("serializer: load_from() not supported on const references"); }
};

/// Sending a serializer-wrapped value to an output stream
inline std::ostream& operator<<(std::ostream &out, const serializer_base &s) {
    s.store_to(out);
    return out;
}

/// Reads a serialized value from an input stream
inline std::istream& operator>>(std::istream &in, serializer_base &s) {
    s.load_from(in);
    return in;
}

/// Read a serialized value from an input stream, rvalue version
inline std::istream& operator>>(std::istream &in, serializer_base &&s) {
    s.load_from(in);
    return in;
}

template <typename T> using EnableFixedArithmetic = std::enable_if_t<
    !std::is_const<T>::value && std::is_arithmetic<T>::value>;
template <typename T> using EnableFixedConstArithmetic = std::enable_if_t<
    std::is_const<T>::value && std::is_arithmetic<T>::value>;

#if not(defined(BOOST_LITTLE_ENDIAN) || defined(BOOST_BIG_ENDIAN))
#error System endianness not supported (neither big-byte nor little-byte endianness detected)!
#endif

/// Specialization for non-const, arithmetic types
template <typename T>
class serializer<T, EnableFixedArithmetic<T>> : public serializer_base {
public:
    /// Wraps a serializer around a reference.
    explicit serializer(T &var) : ref{var} {}

    /// The fixed size of this serialization
    static constexpr size_t size = sizeof(T);

    /** Serializes the data by writing it as bytes and copying it to the given ostream in
     * little-endian order.
     */
    inline void store_to(std::ostream &os) const override {
        const char *in = reinterpret_cast<const char*>(&ref);
#if defined(BOOST_LITTLE_ENDIAN)
        os.write(in, sizeof(T));
#else
        char out[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); i++) out[i] = in[sizeof(T) - i - 1u];
        os.write(out, sizeof(T));
#endif
    }
    /** Deserializes the data by reading it from the given istream and storing it in the wrapped
     * reference.
     */
    inline void load_from(std::istream &is) override {
        char *val = reinterpret_cast<char*>(&ref);
#if defined(BOOST_LITTLE_ENDIAN)
        is.read(val, sizeof(T));
#else
        char val_le[sizeof(T)];
        is.read(val_le, sizeof(T));
        for (size_t i = 0; i < sizeof(T); i++) val[i] = val_le[sizeof(T) - i - 1u];
#endif
    }
private:
    T &ref;
};

/// Specialization for const arithemetic types.  The read method raises an exception.
template <typename T>
class serializer<T, EnableFixedConstArithmetic<T>> : public const_serializer_base {
public:
    explicit serializer(T &var) : ref{var} {}

    /// The fixed size of this serialization
    static constexpr size_t size = sizeof(T);

    inline void store_to(std::ostream &os) const override {
        const char *in = reinterpret_cast<const char*>(&ref);
#if defined(BOOST_LITTLE_ENDIAN)
        os.write(in, sizeof(T));
#else
        char out[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); i++) out[i] = in[sizeof(T) - i - 1u];
        os.write(out, sizeof(T));
#endif
    }
private:
    T &ref;
};

/** Specialization for std::array; the type of elements stored by the array must themselves have
 * serialize<T> specializations.
 *
 * Note that the given array is not stored directly, but references to its elements are, and so the
 * array must stay alive for the lifetime of the serializer instance.
 */
template <typename T, size_t N>
class serializer<std::array<T, N>> : public serializer_base {
    template <size_t... Index>
    serializer(std::array<T, N> &var, index_sequence<Index...>)
    : serializers{{serializer<T>(std::get<Index>(var))...}}
    {}
public:
    /// Wraps a serializer around a reference
    explicit serializer(std::array<T, N> &var) : serializer(var, make_index_sequence<N>()) {}

    /// The fixed size of this serialization
    static constexpr size_t size = N * serializer<T>::size;

    /** Serializes the data by serializing each array element in sequence. */
    inline void store_to(std::ostream &os) const override {
        for (auto &s : serializers) s.store_to(os);
    }
    /** Deserializes the data by reading the individual serialized values from the given istream and
     * storing them, in order, in the referenced array.
     */
    inline void load_from(std::istream &is) override {
        for (auto &s : serializers) s.load_from(is);
    }

private:
    std::array<serializer<T>, N> serializers;
};

/// Returns the sum of all template arguments if all are strictly positive; 0 if any values equal 0.
template <size_t... S> struct nonzero_sum : std::integral_constant<size_t, 0> {};
template <size_t S, size_t... More> struct nonzero_sum<S, More...>
    : std::integral_constant<size_t, (S > 0) ? S + nonzero_sum<More...>::value : 0> {};

/// Apply a function over each element of a parameter pack
#ifdef __cpp_fold_expressions
#define ERIS_SERIALIZER_EXPAND_SIDE_EFFECTS(PATTERN) (((PATTERN), void()), ...)
#else
using expand_side_effects = bool[];
#define ERIS_SERIALIZER_EXPAND_SIDE_EFFECTS(PATTERN) eris::serialize::expand_side_effects{ ((PATTERN), void(), false)..., false }
#endif

/** Specialization for std::pair<A, B> and std::tuple<A, B, ...>.  Each type must have serialize<T>
 * specializations.
 *
 * This holds a reference to the given pair; it should stay alive for the duration of the
 * serialization object.
 */
template <template<typename...> class TupleType, typename... Ts>
class tuple_serializer : public serializer_base {
    using Tuple = TupleType<Ts...>;
public:
    /// Wraps a serializer around a reference
    explicit tuple_serializer(Tuple &var) : tuple_serializer(var, std::index_sequence_for<Ts...>{}) {}

    template <size_t... Ix>
    tuple_serializer(Tuple &var, std::index_sequence<Ix...>) :
        serializers{serializer<Ts>(std::get<Ix>(var))...}
    {}

    /// If both pair types have fixed sizes, this is the sum of those sizes, else 0 (i.e. variable size)
    static constexpr size_t size = nonzero_sum<serializer<Ts>::size...>::value;

    /** Serializes the data by serializing each array element in sequence. */
    inline void store_to(std::ostream &os) const override {
        store_to_impl(os, std::index_sequence_for<Ts...>{});
    }
    /** Deserializes the data by reading the individual serialized values from the given istream and
     * storing them, in order, in the referenced array.
     */
    inline void load_from(std::istream &is) override {
        load_from_impl(is, std::index_sequence_for<Ts...>{});
    }
private:
    template <size_t... Ix>
    void store_to_impl(std::ostream &os, std::index_sequence<Ix...>) const {
        ERIS_SERIALIZER_EXPAND_SIDE_EFFECTS(std::get<Ix>(serializers).store_to(os));
    }

    template <size_t... Ix>
    void load_from_impl(std::istream &is, std::index_sequence<Ix...>) {
        ERIS_SERIALIZER_EXPAND_SIDE_EFFECTS(std::get<Ix>(serializers).load_from(is));
    }
    TupleType<serializer<Ts>...> serializers;
};

template <typename T1, typename T2>
class serializer<std::pair<T1, T2>> : public tuple_serializer<std::pair, T1, T2> {
    using tuple_serializer<std::pair, T1, T2>::tuple_serializer;
};

template <typename... Ts>
class serializer<std::tuple<Ts...>> : public tuple_serializer<std::tuple, Ts...> {
    using tuple_serializer<std::tuple, Ts...>::tuple_serializer;
};


}}
