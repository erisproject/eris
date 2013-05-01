#pragma once
#include <memory>
#include "types.hpp"
#include "Simulation.hpp"

class Simulation;

/* Base class for Agent objects. */
class Agent {
    protected:
        eris_id_t id;
        std::weak_ptr<Simulation> simulator;
        inline bool operator < (Agent other) { return id < other.id; }
        friend Simulation;
};
