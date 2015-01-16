#pragma once
#include <random>
#include <thread>
#include <mutex>
#include <eris/noncopyable.hpp>

namespace eris {

/** Random number generator class for Eris.  This class is responsible for providing a seeded
 * mt19937_64 random number generator, where the seed can be specified explicitly with the `seed(s)`
 * method, and other comes from the environment variable ERIS_RNG_SEED, if set, and otherwise is
 * chosen randomly using std::random_device.  The only publically exposed methods are the (static)
 * Random::rng() and Random::seed() methods.  The generator is only created (and seeded) the first
 * time rng() is called; subsequent calls simply return the existing generator.
 *
 * When using threads, each new thread gets its own random-number generator with a distinct seed.
 * When `seed(s)` is not used and `ERIS_RNG_SEED` is not set, each thread gets its own random seed
 * via the system-specific std::random_device interface.  When an explicit `S` seed is given, the
 * first thread needing a seed uses `S`, the second uses `S + 1`, the third gets `S + 2`, etc.  Note
 * however that specifying a seed is unlikely to be useful in a multithreaded environment where
 * there are interactions between threads: inherent system scheduling randomness means that thread
 * operations will not occur in a reproducible order: if the ordering can affect results, even using
 * the same seed will produce different outcomes.
 */
class Random final {
    public:
        /** Wrapper class around the RNG type used by this class, currently std::mt19937_64.  The
         * wrapper is non-copyable, thus ensuring that the RNG isn't accidentally copied.
         */
        class rng_t : public std::mt19937_64, private eris::noncopyable {};

        /** Returns a random number generator.  The first time this is called, it creates a static
         * random number generator; subsequent calls return the same RNG.  Currently this creates a
         * mt19937_64 rng, which offers a good balance of randomness and performance.
         *
         * The returned rng is suitable for passing into a distribution, such as those provided in
         * the standard \<random\> library.
         *
         * Example:
         *
         *     // Generate random integer in [0,9]
         *     std::uniform_int_distribution<unsigned int> unif(0, 9);
         *     int lucky = unif(Random::rng());
         *
         *     // Generate a random draw from a student-t distribution with 30 d.f.
         *     std::student_t_distribution t30gen(30);
         *     double draw = t30gen(Random::rng());
         *
         * If drawing multiple times, it is recommended (for performance reasons) to obtain a
         * reference to the rng just once, such as:
         *
         *     auto &rng = Random::rng();
         *     std::uniform_int_distribution<unsigned int> unif(0, 9);
         *     for (...) {
         *         unsigned int r = unif(rng);
         *         // ...
         *     }
         */
        inline static rng_t& rng() {
            if (!seeded_) seed();
            return rng_;
        }

        /** A pre-defined, per-thread, reusable N(0,1) distribution.  Using this is preferred to
         * using your own when requesting only a single value as normal values are typically
         * generated in pairs (thus using this will be faster for every second call).
         *
         * \sa rstdnorm()
         */
        thread_local static std::normal_distribution<double> stdnorm;

        /** Returns a draw from a double N(0,1) distribution using the current rng().  Equivalent
         * to: `eris::Random::stdnorm(eris::Random::rng())`.  If drawing multiple values at once, it
         * is preferrable to either use stdnorm directly, or to use your own
         * std::normal_distribution<double> object.
         */
        inline static double rstdnorm() {
            return stdnorm(rng());
        }

        /** Returns the initial seed used for the current thread's random number generator.  If the
         * current thread's RNG is not yet seeded, it is seeded with the initial seed before
         * returning.
         *
         * Note that the returned seed is for the initial state, but the current rng may well no
         * longer be at that initial state.
         *
         * The seed can be specified in two ways: by an explicit call to seed(s), or by setting
         * ERIS_RNG_SEED.  The latter will be used if no seed has been set explicitly via the former
         * the first time rng() is called by any thread.
         *
         * If a seed is explicitly specified, it is used by the first thread to call rng(), and
         * stored as a base seed for other threads (if any).  Subsequent threads then use this base
         * value plus `n` for a seed, where `n` is the number of threads having previously generated
         * a seed.  In other words, if `ERIS_RNG_SEED=10` (and `Random::seed(s)` is not called) then
         * the first thread to generate a seed gets 10, the second gets 11, and so on.
         *
         * If no seed is explicitly specified by either `ERIS_RNG_SEED` or `Random::seed(s)`, all
         * threads get a random seed (each using the system's default std::random_device when the
         * seed is generated).
         */
        static const rng_t::result_type& seed();

        /** Sets the RNG seed to the given value.  This is allowed as long as no seed has been set
         * (including implicitly by making a call to rng()) at all, or rng() hasn't been called from
         * another thread.  (The latter restriction is in place because the seed is used as the
         * basis for thread seeds, but thread variables are local to the calling threads and cannot be
         * accessed from other threads).
         *
         * The given seed will be used for other threads as well: the first thread to request a seed
         * uses `seed + 1`, the second uses `seed + 2`, etc.  Note, however, that inherent system
         * scheduling randomness makes multithreaded randomness unlikely to be reproducible even if
         * started with the same seed, when there are dependencies between threads.
         *
         * Throws std::runtime_error if any thread other than the caller already has a random seed.
         */
        static void seed(rng_t::result_type seed);

    private:
        // Lock controlling the init_* parameters
        static std::mutex lock_;
        // True if seed initialization has been done
        static bool init_done_;
        // If true, use init_base_ + init_count_; otherwise get a random seed
        static bool init_use_base_;
        // The base seed to use for new threads' RNGs (if init_use_base_ is true)
        static rng_t::result_type init_base_;
        // The offset to add to the base for new threads' RNGs.  Incremented for each new thread.
        static unsigned int init_count_;

        // Thread-specific variables:
        thread_local static bool seeded_;
        thread_local static rng_t rng_;
        thread_local static rng_t::result_type seed_;
};

}
