#pragma once
#include <random>

namespace eris {

/** Random number generator class for Eris.  This class is responsible for providing a seeded
 * mt19937_64 random number generator, where the seed comes from ERIS_RNG_SEED, if defined, and
 * otherwise is chosen randomly.  The only publically exposed method is the (static) Random::rng()
 * method, which will return an appropriately seeded random number generator.  The generator is only
 * created (and seeded) the first time rng() is called; subsequent calls simply return the existing
 * generator.
 */
class Random final {
    public:
        /** Returns a random number generator.  The first time this is called, it creates a static
         * random number generator; subsequent calls return the same RNG.  Currently this creates a
         * mt19937_64 rng, which offers a good balance of randomness and performance.
         *
         * The returned rng is suitable for passing into a distribution, such as those provided in
         * <random>.
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
        inline static std::mt19937_64& rng() {
            if (!_rng) _rng = new std::mt19937_64(seed());
            return *_rng;
        }
        /** Returns the seed used for this RNG.  If no seed has been established yet, this uses the
         * environment variable ERIS_RNG_SEED.  If that isn't set, it attempts to generate one in a
         * unique way using the current process and/or system environment.
         */
        inline static const std::mt19937_64::result_type seed() {
            if (!_seed) _reseed();
            return _seed;
        }
    private:
        static std::mt19937_64 *_rng;
        static std::mt19937_64::result_type _seed;
        static std::mt19937_64::result_type _reseed();
};

}
