#pragma once
#include <memory>

/** The top-level eris namespace is the namespace containing all of the eris core functionality.
 * Non-core functionality, such as specific implementations of different types of objects, goes into
 * nested namespaces such as eris::consumer, eris::firm, eris::market, eris::optimizer.
 */
namespace eris {

/** Top-level wrapper class around a Simulation.  Simulation objects should not be
 * created directly (as they need to be shared by simulation component classes,
 * and thus require dealing with strong and weak references).
 */

template <class T>
class Eris {
    public:
        /** Generic constructor, creates a new T object.  T must have a public
         * 0-argument constructor.
         */
        Eris() : t(std::shared_ptr<T>(new T)) {}

        /** For more complicated construction, declare variable with: `Eris e(new T(...))` */
        Eris(T *s) : t(std::shared_ptr<T>(s)) {}

        virtual ~Eris() = default;

        /** Deferenced member access gets redirected through the Simulation object */
        T* operator ->() { return t.get(); }
    private:
        std::shared_ptr<T> t;
};

}
