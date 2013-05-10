#include <eris/Random.hpp>
#include <eris/cycle.h>
#include <string>
#include <cstdlib>

namespace eris {

std::mt19937_64 *Random::_rng = nullptr;
std::mt19937_64::result_type Random::_seed = 0;

std::mt19937_64::result_type Random::_reseed() {
    _seed = 0;
    char *envseed = getenv("ERIS_RNG_SEED");
    if (envseed)
        try { _seed = std::stoull(envseed); } catch (std::exception e) {}

    if (!_seed) {
#ifdef HAVE_TICK_COUNTER
        _seed = getticks();
#else
        // The above is more random, but this shouldn't be too terrible:
        _seed ||= srand((time(NULL) & 0xFFFF) | (getpid() << 16));
#endif
    }
    return _seed;
}

}
