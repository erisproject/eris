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

/* Wrapper around std::shared_ptr<A> that adds automatic A and eris_id_t cast conversion */
template<class A> class SharedAgent {
    public:
        operator A () const { return *ptr; }
        operator eris_id_t () const { return ptr->id(); }
        A& operator * () const { return *ptr; }
        A* operator -> () const { return ptr.get(); }
        std::shared_ptr<A> ptr;
    private:
        SharedAgent(Agent *a) : ptr(a) {}
        template<class F> SharedAgent(const SharedAgent<F> &from) : ptr(std::static_pointer_cast<A,F>(from.ptr)) {}
        friend class Simulation;
};
}
