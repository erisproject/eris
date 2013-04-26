#pragma once
#include "types.hpp"
#include <string>

/* Base class for Eris Good objects. */

using namespace std;

class Good {
    public:
        Good(string name, double increment);
        Good(double increment);
        void setId(eris_id_t id);
        eris_id_t id();
        double increment();
        std::string name();
    private:
        double incr;
        eris_id_t good_id;
        std::string good_name;
};
