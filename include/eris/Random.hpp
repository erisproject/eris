#pragma once
#include <random>

// Random number generator class for Eris.  This class is responsible for providing a seeded
// mt19937_64 random number generator, where the seed comes from ERIS_RNG_SEED, if defined, and
// otherwise is chosen randomly.  The only publically exposed method is the (static) Random::rng()
// method, which will return an appropriately seeded random number generator.  The generator is only
// created (and seeded) the first time rng() is called; subsequent calls simply return the existing
// generator.
//
// To properly use this class, inherit from it, then call rng().
//

namespace eris {

class Random {
    public:
        inline static std::mt19937_64& rng() {
            if (!_rng) _rng = new std::mt19937_64(seed());
            return *_rng;
        }
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
