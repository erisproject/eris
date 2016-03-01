#include <eris/random/distribution.hpp>

namespace eris { namespace random {

thread_local std::normal_distribution<double> stdnorm{0.0, 1.0};

}}
