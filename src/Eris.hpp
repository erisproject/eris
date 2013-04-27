#pragma once
#include <memory>

// Top-level wrapper class around a Simulation.  Simulations should not be
// created directly (as they need to be shared by simulation component classes,
// and thus require dealing with strong and weak references).

template <class T>

class Eris {
    public:
        // Generic constructor, creates a new T object.  T must have a public
        // 0-argument constructor.
        Eris() { t = std::shared_ptr<T>(new T); }
        // For more complicated construction, use: Eris e(new T(...))
        Eris(T *s) { t = std::shared_ptr<T>(s); }
        // Deferenced member access gets redirected through the Simulation object
        T* operator ->() { return t.get(); }
    private:
        std::shared_ptr<T> t;
};
