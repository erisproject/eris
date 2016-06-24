#pragma once
#include <cstdint>
#include <iostream>
#include <boost/detail/endian.hpp>

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
 * If the serialized type has a constant size, it should be exposed by declaring a static `size`
 * member indicating the size; this will allow it to be used in places such as
 * Serializer::addHeaderField, which only allow constant-sized serialization values.
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
protected:
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
std::ostream& operator<<(std::ostream &out, const serializer_base &s);

/// Reads a serialized value from an input stream
std::istream& operator>>(std::istream &in, serializer_base &s);

/// Read a serialized value from an input stream, rvalue version
std::istream& operator>>(std::istream &in, serializer_base &&s);

template <typename T> using EnableFixedArithmetic = typename std::enable_if<
    not std::is_const<T>::value and
    std::is_arithmetic<T>::value>::type;
template <typename T> using EnableFixedConstArithmetic = typename std::enable_if<
    std::is_const<T>::value and
    std::is_arithmetic<T>::value>::type;

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

protected:
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

protected:
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

}}
