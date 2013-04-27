#pragma once
#include "types.hpp"
#include <string>

/* Base class for Eris Good objects. */

class Good {
    public:
        Good(std::string name, double increment);
        Good(double increment) : Good("", increment) {};
        Good() : Good(0.0) {};
        std::string name();
        void setName(std::string name);
        double increment();
        void setIncrement(double increment);
    private:
        double incr;
        std::string good_name;
};
