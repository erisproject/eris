#pragma once
#include <random>
#include <thread>
#include <mutex>
#include <memory>

namespace eris {

/** Random number generator class for Eris.  This class is responsible for providing a seeded
 * mt19937_64 random number generator, where the seed comes from ERIS_RNG_SEED, if defined, and
 * otherwise is chosen randomly.  The only publically exposed methods are the (static) Random::rng()
 * and Random::seed() methods.  The generator is only created (and seeded) the first time rng() is
 * called; subsequent calls simply return the existing generator.
 *
 * When using threads, each new thread gets its own random-number generator.  When ERIS_RNG_SEED is
 * not specified, each thread gets its own random seed.  When ERIS_RNG_SEED is specified, the first
 * thread needing a seed uses `ERIS_RNG_SEED`, the second uses `ERIS_RNG_SEED + 1`, the third gets
 * `ERIS_RNG_SEED + 2`, etc.
 */
class Random final {
    public:
        /** typedef for the RNG type used by this class, currently std::mt19937_64.
         */
        typedef std::mt19937_64 rng_t;

        /** Returns a random number generator.  The first time this is called, it creates a static
         * random number generator; subsequent calls return the same RNG.  Currently this creates a
         * mt19937_64 rng, which offers a good balance of randomness and performance.
         *
         * The returned rng is suitable for passing into a distribution, such as those provided in
         * \<random\>.
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
         */
        inline static rng_t& rng() {
            if (!rng_) rng_ = std::unique_ptr<rng_t>(new rng_t(seed()));
            return *rng_;
        }

        /** Returns the seed used (for the current thread, if using threads).  If no seed has been
         * established yet, a new one is generated and stored before being returned.
         *
         * The first thread to generate a seed checks the environment variable ERIS_RNG_SEED.  If
         * it is set, it is stored as a base seed and used as the seed for the calling thread.
         * Subsequent threads then use `ERIS_RNG_SEED + n` for a seed, where `n` is the number of threads
         * having generated a seed before the current one.  In other words, if ERIS_RNG_SEED=10 then
         * the first thread to generate a seed gets 10, the second gets 11, and so on.
         *
         * If ERIS_RNG_SEED is not set, all threads get a random seed (each using the system's default
         * std::random_device when the seed is generated).  Note that ERIS_RNG_SEED is only checked
         * by the first thread needing a seed.
         *
         * Note that ERIS_RNG_SEED is only checked once: subsequent threads use its existance and
         * value at the time the first thread checked it.
         */
        static const rng_t::result_type& seed();

    private:
        // Lock controlling the init_* parameters
        static std::mutex lock_;
        // True if seed initialization has been done
        static bool init_done_;
        // If true, use init_base_ + init_count_; otherwise get a random seed
        static bool init_use_base_;
        // The base seed to use for new threads' RNGs
        static rng_t::result_type init_base_;
        // The offset to add to the base for new threads' RNGs.  Incremented for each new thread.
        static unsigned int init_count_;

        // Pointer to the random number generator
        thread_local static bool seeded_;
        thread_local static std::unique_ptr<rng_t> rng_;
        thread_local static rng_t::result_type seed_;
};

}
