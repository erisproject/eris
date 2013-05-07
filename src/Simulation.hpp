#pragma once
#include "types.hpp"
#include "Agent.hpp"
#include "Good.hpp"
#include <map>
#include <type_traits>
#include <exception>

namespace eris {


/* This class is at the centre of an Eris economy model; it keeps track of all
 * of the agents currently in the economy, all of the goods currently available
 * in the economy, and the interaction mechanisms (e.g. markets).  Note that
 * all of these can change from one period to the next.  It's also responsible
 * for dispatching interactions (e.g. letting markets operate) and any
 * iteration-sensitive agent events (e.g. aging/dying/etc.).
 *
 * In short, this is the central piece of the Eris framework that dictates how
 * all the other pieces interact.
 */
class Simulation : public std::enable_shared_from_this<Simulation> {
    public:
        //Simulation();
        virtual ~Simulation() = default;

        // Wrapper around std::shared_ptr<G> that adds automatic G and eris_id_t cast conversion
        template<class T> class SharedObject {
            public:
                virtual ~SharedObject() = default;
                // Using as an A gives you the underlying A object:
                virtual operator T () const { return *ptr; }
                // Using as an eris_id_t gives you the object's id
                virtual operator eris_id_t () const { return ptr->id(); }
                // Dereferencing gives you the underlying A
                virtual T& operator * () const { return *ptr; }
                // Dereferencing member access works on the underlying A
                virtual T* operator -> () const { return ptr.get(); }

                // The underlying shared_ptr
                const std::shared_ptr<T> ptr;

                // Constructing a new SharedObject<A> using an existing SharedObject<F> recasts the F pointer
                // to an A pointer.  This constructor also allows you to do things like:
                // SharedObject<Subclass> = something_returning_SharedObject<Object>(...)
                template<class F> SharedObject(const SharedObject<F> &from) : ptr(std::static_pointer_cast<T,F>(from.ptr)) {}
            private:
                SharedObject(T *g) : ptr(g) {}
                friend class Simulation;
        };

        typedef std::map<eris_id_t, SharedObject<Good>> GoodMap;
        typedef std::map<eris_id_t, SharedObject<Agent>> AgentMap;

        SharedObject<Agent> agent(eris_id_t aid);
        SharedObject<Good> good(eris_id_t gid);


        template <class A> SharedObject<A> addAgent(A a);
        template <class G> SharedObject<G> addGood(G g);
        void removeAgent(eris_id_t aid);
        void removeGood(eris_id_t gid);
        void removeAgent(const Agent &a);
        void removeGood(const Good &g);
        const AgentMap agents();
        const GoodMap goods();

        class already_owned : public std::invalid_argument {
            public:
                already_owned(const std::string &objType, std::shared_ptr<Simulation> sim) :
                    std::invalid_argument(objType + " belongs to another Simulation"),
                    simulation(sim) {}
                const std::shared_ptr<Simulation> simulation;
        };
        class already_added : public std::invalid_argument {
            public:
                already_added(const std::string &objType) : std::invalid_argument(objType + " already belongs to this Simulation") {}
        };

    private:
        void insertAgent(const SharedObject<Agent> &agent);
        void insertGood(const SharedObject<Good> &good);
        eris_id_t agent_id_next = 1, good_id_next = 1;
        AgentMap agent_map;
        GoodMap good_map;
};

// Copies and stores the passed in Agent and returns a shared pointer to it.  Will throw a
// Simulation::already_owned exception if the agent belongs to another Simulation, and a
// Simulation::already_added exception if the agent has already been added to this Simulation.
template <class A> Simulation::SharedObject<A> Simulation::addAgent(A a) {
    // This will fail if A isn't an Agent (sub)class:
    Simulation::SharedObject<Agent> agent(new A(std::move(a)));
    insertAgent(agent);
    return agent;
}

// Copies and stores the passed-in Good, and returned a shared pointer to it.
template <class G> Simulation::SharedObject<G> Simulation::addGood(G g) {
    // This will fail if G isn't a Good (sub)class:
    Simulation::SharedObject<Good> good(new G(std::move(g)));
    insertGood(good);
    return good;
}




}
