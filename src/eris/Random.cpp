#include <eris/Random.hpp>
#include <string>
#include <cstdlib>

namespace eris {

std::mutex Random::lock_;
bool Random::init_done_ = false;
bool Random::init_use_base_;
Random::rng_t::result_type Random::init_base_;
unsigned int Random::init_count_ = 0;

thread_local bool Random::seeded_ = false;
thread_local Random::rng_t Random::rng_;
thread_local Random::rng_t::result_type Random::seed_;




const Random::rng_t::result_type& Random::seed() {
    if (!seeded_) {
        // Lock out other threads: we're checking/setting global variables here
        std::unique_lock<std::mutex> lock(lock_);
        if (!init_done_) {
            char *envseed = getenv("ERIS_RNG_SEED");
            if (envseed) {
                std::string seedstr(envseed);
                if (seedstr != "") {
                    init_base_ = std::stoull(seedstr); // Could throw
                    init_use_base_ = true;
                    init_count_ = 0;
                }
            }
            // If no ERIS_RNG_SEED, init_use_base_ stays false

            init_done_ = true;
        }

        seed_ = init_use_base_
            ? init_base_ + init_count_
            : std::random_device{}();
        init_count_++;
        seeded_ = true;
    }

    return seed_;
}

void Random::seed(rng_t::result_type s) {
    // We need to check the global variables, so lock out other threads
    std::unique_lock<std::mutex> lock(lock_);
    // If multiple threads (or a single thread other than the current thread) have been seeded, abort
    if (init_count_ > 1 or (init_count_ == 1 and not seeded_))
        throw std::runtime_error("Random::seed(s) called too late: multiple threads have been seeded");

    // Set (or reset) the seed for the current thread:
    seed_ = s;
    rng_.seed(seed_);

    // Store the seed as a base for any other threads, starting from (seed+1), then (seed+2), etc.
    init_base_ = s;
    init_use_base_ = true;
    init_count_ = 1;
    init_done_ = true;
}

}
