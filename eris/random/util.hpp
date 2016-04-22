#pragma once
#include <eris/random/rng.hpp>
#include <eris/random/normal_distribution.hpp>
#include <boost/random/uniform_real_distribution.hpp>

// Utility/shortcut methods that don't fit nicely elsewhere.

namespace eris { namespace random {

/** Convenience method for obtaining a double draw from a normal distribution using
 * eris::random::rng().
 *
 * The current implementation uses eris::random::normal_distribution for draw, but that could
 * change.
 *
 * \param mean the mean of the normal distribution; defaults to 0 if omitted.
 * \param stdev the standard deviation of the normal distribution; defaults to 1 if omitted.
 */
inline double rnormal(double mean = 0.0, double stdev = 1.0) {
    return normal_distribution<double>(mean, stdev)(rng());
}

/// Compatibility function for pre-1.0 eris; use rnormal() instead.
inline double rstdnorm() { return rnormal(); }

/** Convenience method for obtaining a double draw from a uniform [a,b) distribution using
 * eris::random::rng() and boost::random::uniform_real_distribution<double>.
 *
 * \param a the minimum value; defaults to 0 if omitted
 * \param b the maximum (actually, the supremum) value; defaults to 1 if omitted
 */
inline double runiform(double a = 0.0, double b = 1.0) {
    return boost::random::uniform_real_distribution<double>(a, b)(rng());
}

}}
