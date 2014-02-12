#include <eris/interopt/Factory.hpp>

namespace eris { namespace interopt {

void Factory::interOptimize() {
    action_ = needAction();
}

void Factory::interApply() {
    if (action_ > 0)
        create(action_);
    else if (action_ < 0)
        destroy(-action_);
}

} }
