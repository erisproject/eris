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
        // Using as an A gives you the underlying A object:
        operator A () const { return *ptr; }
        // Using as an eris_id_t gives you the object's id
        operator eris_id_t () const { return ptr->id(); }
        // Dereferencing gives you the underlying A
        A& operator * () const { return *ptr; }
        // Dereferencing member access works on the underlying A
        A* operator -> () const { return ptr.get(); }
        std::shared_ptr<A> ptr;
        // Constructing a new SharedAgent<A> using an existing SharedAgent<F> recasts the F pointer
        // to an A pointer.  This constructor also allows you to do things like:
        // SharedAgent<Subclass> = something_returning_SharedAgent<Agent>(...)
        template<class F> SharedAgent(const SharedAgent<F> &from) : ptr(std::static_pointer_cast<A,F>(from.ptr)) {}

    private:
        SharedAgent(Agent *a) : ptr(a) {}
        friend class Simulation;
};
}
