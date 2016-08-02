#pragma once
#pragma message "eris::belief::BayesianLinear is deprecated; use eris::learning::BayesianLinear instead"
#include <eris/learning/BayesianLinear.hpp>

namespace eris { namespace belief = ::eris::learning; }
