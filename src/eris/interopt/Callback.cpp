#include <eris/interopt/Callback.hpp>
#include <algorithm>

namespace eris { namespace interopt {

#define INTEROPT_CALLBACK_METHODS(TYPE)\
TYPE##Callback::TYPE##Callback(std::function<void()> callback) : callback_{std::move(callback)} {}\
void TYPE##Callback::inter##TYPE() { return callback_(); }

INTEROPT_CALLBACK_METHODS(Begin)
INTEROPT_CALLBACK_METHODS(Optimize)
INTEROPT_CALLBACK_METHODS(Apply)
INTEROPT_CALLBACK_METHODS(Advance)
#undef INTEROPT_CALLBACK_METHODS

}}

