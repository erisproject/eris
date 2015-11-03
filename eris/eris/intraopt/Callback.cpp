#include <eris/intraopt/Callback.hpp>
#include <algorithm>

namespace eris { namespace intraopt {

#define INTRAOPT_CALLBACK_METHODS(TYPE, RETURN)\
TYPE##Callback::TYPE##Callback(std::function<RETURN()> callback) : callback_{std::move(callback)} {}\
RETURN TYPE##Callback::intra##TYPE() { return callback_(); }

INTRAOPT_CALLBACK_METHODS(Initialize, void)
INTRAOPT_CALLBACK_METHODS(Reset, void)
INTRAOPT_CALLBACK_METHODS(Optimize, void)
INTRAOPT_CALLBACK_METHODS(Reoptimize, bool)
INTRAOPT_CALLBACK_METHODS(Apply, void)
INTRAOPT_CALLBACK_METHODS(Finish, void)
#undef INTRAOPT_CALLBACK_METHODS

}}
