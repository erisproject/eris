#pragma once
#include "types.hpp"
#include "Agent.hpp"

class Consumer : public Agent {
    public:
        virtual double utility(Bundle b);
};
