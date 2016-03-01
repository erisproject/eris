#include <eris/random/rng.hpp>
#include <mutex>
#include <cmath>
#include <limits>
#include <type_traits>

namespace eris { namespace random {

std::mutex lock_;
bool init_done_ = false;
rng_t::result_type last_thr_seed_;

thread_local rng_t rng_;
thread_local bool seeded_ = false;
thread_local rng_t::result_type seed_;

const rng_t::result_type& seed() {
    if (!seeded_) {
        {
            // Lock out other threads: we're checking/setting global variables here
            std::unique_lock<std::mutex> lock(lock_);
            if (!init_done_) {
                // We're the first thread to set or need a seed

                // First check to see if the ERIS_RNG_SEED environment variable is given:
                char *envseed = getenv("ERIS_RNG_SEED");
                bool found_env = false;
                if (envseed) {
                    std::string seedstr(envseed);
                    if (seedstr != "") {
                        last_thr_seed_ = std::stoull(seedstr); // Could throw (don't catch it)
                        found_env = true;
                    }
                }

                if (!found_env) {
                    // No (or empty) ERIS_RNG_SEED: get a random seed from the OS
                    last_thr_seed_ = std::random_device{}();
                }

                seed_ = last_thr_seed_;
                init_done_ = true;
            }
            else {
                seed_ = ++last_thr_seed_;
            }
        } // Release lock

        rng_.seed(seed_);
        seeded_ = true;
    }

    return seed_;
}

void seed(rng_t::result_type s) {
    {
        // If seed initialization hasn't been done by any thread, set the base seed other threads
        // will use:
        std::unique_lock<std::mutex> lock(lock_);
        if (!init_done_) {
            last_thr_seed_ = s;
            init_done_ = true;
        }
    }

    // Set (or reset) the seed for the current thread:
    seed_ = s;
    rng_.seed(seed_);
    seeded_ = true;
}

}}
