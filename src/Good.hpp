#pragma once
#include "types.hpp"
#include <string>

namespace eris {

/* Base class for Eris Good objects.  Goods should generally be a subclass of Good, typically either
 * a Good::Continuous or Good::Discrete, also defined here.
 */

class Simulation;

class Good {
    public:
        virtual ~Good() = default;

        virtual double increment() = 0;
        eris_id_t id() const { return _id; }
        operator eris_id_t() const { return _id; }

        std::string name;

        class Continuous;
        class Discrete;
    protected:
        Good(std::string name);
        eris_id_t _id = 0;
        std::weak_ptr<eris::Simulation> simulator;
        bool operator < (const Good &other) { return id() < other.id(); }
        friend eris::Simulation;
};

/* Continous good.  This is a good with a fixed increment of 0, usable for any
 * good which is (quasi-)infinitely subdividable.
 */
class Good::Continuous : public Good {
    public:
        Continuous(std::string name = "");
        double increment();
};

class Good::Discrete : public Good {
    public:
        Discrete(std::string name = "", double increment = 1.0);
        void setIncrement(double increment);
        double increment();
    private:
        double incr = 1.0;
};

}
