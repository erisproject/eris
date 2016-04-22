#pragma once
#include <eris/random/rng.hpp>
#include <eris/random/util.hpp>
#include <eris/random/truncated_distribution.hpp>

/** \file
 *
 * \deprecated
 *
 * This file is provided for compatibility with pre-1.0.0 code; it simply includes eris/random/rng.hpp,
 * eris/random/util.hpp, and eris/random/truncated_distribution.hpp, and aliases the eris::random
 * namespace to eris::Random.
 *
 * It is only intended for compatibility with eris releases before 1.0.0, which provided a Random
 * "class" which consisting of all static members with the same public interface now provided by functions
 * in the eris::random namespace.
 *
 * New code should not include this file.
 */

namespace eris {
/** \deprecated
 *
 * Namespace alias for pre-1.0.0 versions of eris.  This allows for code compatibility in most
 * cases; before 1.0.0, eris::Random was a class consisting of only static members; in 1.0.0 this
 * changed to static functions in the random namespace.
 */
namespace Random = random;
}
