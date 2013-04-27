#pragma once
#include "types.hpp"
#include "Agent.hpp"
#include "Bundle.hpp"

class Consumer : public Agent {
    public:
        virtual double utility(Bundle b);
    private:
        Bundle income;
};
