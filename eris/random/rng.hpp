#pragma once
#include <boost/random/mersenne_twister.hpp>
#include <eris/noncopyable.hpp>

namespace eris {    
/// Namespace for random number generation and distributions variables and functions
namespace random {

/** The eris RNG class, currently boost::random::mt19937_64.  The wrapper class is non-copyable,
 * thus ensuring that the RNG isn't accidentally copied.
 */
class rng_t : public boost::random::mt19937_64, private eris::noncopyable {};

#ifndef DOXYGEN_SHOULD_SEE_THIS
// Internal, thread-specific variables:
thread_local extern bool seeded_;
thread_local extern rng_t rng_;
#endif

/** Returns the initial seed used for the current thread's random number generator.  If the
 * current thread's RNG is not yet seeded, it is seeded with an initial seed before
 * returning.
 *
 * Note that the returned seed is for the RNG's initial state, but the current rng may well no
 * longer be at that initial state--and in particular, won't be if any random numbers have been
 * generated.
 *
 * When the RNG has not yet been seeded (and either no threads are involved, or no other threads
 * have specified or needed a seed), this method checks the environment variable ERIS_RNG_SEED; if
 * set, the value is used as the RNG seed.  Otherwise, a random seed is obtained from the operating
 * system via std::random_device.
 *
 * When using threads, the initial seed from the RNG within a thread is based on the seed of the
 * very first thread that specified (or obtained) a seed: subsequent threads use a seed incremented
 * from that first thread's seed.  For example, if there are 8 threads with ERIS_RNG_SEED unset and
 * no previous calls to seed() or seed(s) by any thread, the first thread to call rng() or seed()
 * obtains a seed `S` randomly from the system's random device.  The second thread would get an RNG
 * seeded with value `S+1`; the second would get `S+2`; and so on.
 *
 * Threads are, of course, free to set their own seeds according to some other pattern by calling
 * `seed(s)` from within new threads.  Note than only the first seed (whether implicit or explicit)
 * sets the seed base; any subsequent calls to seed(s) are localized to the thread in which the call
 * is made.  For example, if the first thread calls `seed(123)`, the next thread that needs
 * automatic seeding will get seed 124, even if other threads in the meantime specify their own
 * seeds, and even if the first thread resets its seed to some other value.
 */
const rng_t::result_type& seed();

/** Sets (or resets, if already seeded) the RNG seed for the current thread to the given value.  If
 * none of seed(s), seed(), or rng() have been called by this or any other thread, this also
 * establishes the base seed that other threads will increment and use when they need automatic
 * seeding (from a seed() or rng() call without a preceeding seed(s) call).  Otherwise the specified
 * seed affects only the current thread.
 */
void seed(rng_t::result_type seed);

/** Returns the random number generator for the current thread.  If the RNG has not yet been seeded
 * by a call to seed(s) or seed(), seed() is called implicitly to seed the random number generator
 * before returning a reference to it.
 *
 * Currently this returns a mt19937_64 rng, which offers a good balance of randomness and
 * performance, but that may change in the future.
 *
 * The returned rng is suitable for passing into a distribution, such as those provided in the
 * stl \<random\> library and the boost::random library.
 *
 * Example:
 *
 *     using namespace eris::random;
 *     // Generate random integer in [0,9]
 *     boost::random::uniform_int_distribution<unsigned int> unif(0, 9);
 *     int lucky = unif(rng());
 *
 *     // Generate a random draw from a student-t distribution with 30 d.f.
 *     boost::random::student_t_distribution t30gen(30);
 *     double draw = t30gen(rng());
 *
 * If drawing multiple times, it is slightly more efficient to obtain a reference to the rng
 * just once, such as:
 *
 *     auto &rng = eris::random::rng();
 *     boost::random::uniform_int_distribution<unsigned int> unif(0, 9);
 *     for (...) {
 *         unsigned int r = unif(rng);
 *         // ...
 *     }
 *
 * Note, however, that the returned reference is not thread-safe and so must not be shared among
 * threads: each thread should call rng() itself to obtain a thread-local RNG object.
 */
inline static rng_t& rng() {
    if (!seeded_) seed();
    return rng_;
}

}}
