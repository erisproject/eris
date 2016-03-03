#include <eris/random/distribution.hpp>

namespace eris { namespace random {

thread_local boost::random::normal_distribution<double> stdnorm{0.0, 1.0};

}}
