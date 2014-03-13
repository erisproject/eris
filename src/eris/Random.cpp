#include <eris/Random.hpp>
#include <string>
#include <cstdlib>

namespace eris {

const std::mt19937_64::result_type& Random::seed() {
    if (!seeded_) {
        // Lock out other threads: we're setting the global things
        lock_.lock();
        if (!init_done_) {
            char *envseed = getenv("ERIS_RNG_SEED");
            if (envseed) {
                std::string seedstr(envseed);
                if (seedstr != "") {
                    init_base_ = std::stoull(seedstr);
                    init_use_base_ = true;
                    init_count_ = 0;
                }
            }
            // If no ERIS_RNG_SEED, init_base_base_ stays false

            init_done_ = true;
        }

        seed_ = init_use_base_
            ? init_base_ + init_count_++
            : std::random_device{}();
        seeded_ = true;
    }

    return seed_;
}

}
