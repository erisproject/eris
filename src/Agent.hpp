#pragma once
#include <memory>
#include "types.hpp"
#include "Simulation.hpp"

class Simulation;

/* Base class for Agent objects. */
class Agent {
    public:
        void setSim(std::shared_ptr<Simulation> s);
    protected:
        std::weak_ptr<Simulation> simulator;
};
