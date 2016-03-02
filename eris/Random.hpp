#pragma once
#include <eris/random/rng.hpp>
#include <eris/random/distribution.hpp>

/** \file
 *
 * \deprecated
 *
 * Including this file is simply a shortcut for including eris/random/rng.hpp and
 * eris/random/distribution.hpp, and for providing the eris::Random = eris::random namespace alias.
 * It is mainly provided for compatibility with eris releases before 1.0.0, which provided a Random
 * "class" consisting of all static members with the same public interface now provided by functions
 * in the eris::random namespace.
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
