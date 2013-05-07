#pragma once
#include "types.hpp"
#include <memory>

namespace eris {

class Simulation;

/* Base class for Agent objects. */
class Agent {
    public:
        virtual ~Agent() = default;
        eris_id_t id() const { return _id; }
    protected:
        Agent() {}
        eris_id_t _id;
        std::weak_ptr<eris::Simulation> simulator;
        bool operator < (const Agent &other) { return id() < other.id(); }
        friend eris::Simulation;
};

}
