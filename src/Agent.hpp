#pragma once
#include "types.hpp"
#include "Eris.hpp"

/* Base class for Eris Agent objects. */
class Eris;
class Agent {
    public:
        void setId(eris_id_t id);
        eris_id_t id();
        void setSim(Eris *s);
    protected:
        Eris *simulator;
    private:
        eris_id_t agent_id;
};
