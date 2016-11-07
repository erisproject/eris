#pragma once
#pragma message "eris::belief::BayesianLinearRestricted is deprecated; use eris::learning::BayesianLinearRestricted instead"
#include <eris/learning/BayesianLinearRestricted.hpp>

namespace eris { namespace belief = ::eris::learning; }
