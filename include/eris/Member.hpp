#pragma once
#include <eris/types.hpp>
#include <memory>

// Contains the Member class, which is a very general base class for objects "owned" and stored by a
// Simulation (e.g. goods, agents, markets).  Also contains the SharedMember<M> class, a wrapper
// around a std::shared_ptr<M> that is overloaded to work as an M class as needed.

namespace eris {

class Simulation;

// Base class for "members" of a simulation: goods, agents, markets.  This class provides an
// id, a weak reference to the owning simulation object, and a < operator comparing ids (so
// that map ordering works).
class Member {
    public:
        virtual ~Member() = default;
        eris_id_t id() const { return _id; }
        operator eris_id_t() const { return _id; }
        bool operator < (const Member &other) { return id() < other.id(); }
    protected:
        eris_id_t _id = 0;
        std::weak_ptr<eris::Simulation> simulator;
        friend eris::Simulation;
};

// Wrapper around std::shared_ptr<G> that adds automatic G and eris_id_t cast conversion
template<class T> class SharedMember final {
    public:
        // Using as an A gives you the underlying A object:
        virtual operator T& () const { return *ptr; }
        // Using as an eris_id_t gives you the object's id
        virtual operator eris_id_t () const { return ptr->id(); }
        // Dereferencing gives you the underlying A
        virtual T& operator * () const { return *ptr; }
        // Dereferencing member access works on the underlying A
        virtual T* operator -> () const { return ptr.get(); }

        // The underlying shared_ptr
        const std::shared_ptr<T> ptr;

        // Constructing a new SharedMember<A> using an existing SharedMember<F> recasts the F pointer
        // to an A pointer.  This constructor also allows you to do things like:
        // SharedMember<A> ... = (... SharedMember<B>) when either A or B are subclasses of
        // each other.
        template<class F> SharedMember(const SharedMember<F> &from) : ptr(std::static_pointer_cast<T,F>(from.ptr)) {}
    private:
        SharedMember(T *m) : ptr(m) {}
        friend class Simulation;
};

}
