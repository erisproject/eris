#pragma once
#include <memory>
#include <eris/Simulation.hpp>

/** The top-level eris namespace is the namespace containing all of the eris core functionality.
 * Non-core functionality, such as specific implementations of different types of objects, goes into
 * nested namespaces such as eris::consumer, eris::firm, eris::market, eris::optimizer.
 */
namespace eris {

/** Top-level wrapper class around a Simulation.  Simulation objects should not be created directly
 * (as they need to be shared by simulation component classes, and thus require dealing with strong
 * and weak references).
 *
 * This class is provided as a wrapper making the shared_ptr access slightly more convenient; it can
 * be ignored entirely and replaced with direct shared_ptr<Simulation> objects.
 */

template <class T = Simulation>
class Eris {
    public:
        /** Generic constructor, creates a new T object.  T must have a public
         * 0-argument constructor.
         */
        Eris() : sim_(std::shared_ptr<T>(new T)) {}

        /** For more complicated construction, declare variable with: `Eris e(new T(...))` */
        Eris(T *s) : sim_(std::shared_ptr<T>(s)) {}

        /** Constructing with an already-created shared pointer */
        Eris(std::shared_ptr<T> s) : sim_(s) {}

        virtual ~Eris() = default;

        /** Deferenced member access gets redirected through the Simulation object */
        T* operator ->() { return sim_.get(); }
    private:
        std::shared_ptr<T> sim_;
};

}
