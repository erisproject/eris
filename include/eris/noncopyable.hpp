#pragma once

namespace eris {

/** Simple utility class intended to be inherited (privately) for a class that should not be copied.
 * This class has protected constructors and destructors which are not accessible to non-derived
 * classes.
 *
 * Typical use:
 *
 *     class MyClass : private eris::noncopyable { ... }
 */
class noncopyable {
    protected:
        /// Default empty constructor
        noncopyable() = default;
        /// Default Protected destructor
        ~noncopyable() = default;
        /// Deleted copy constructor
        noncopyable(const noncopyable&) = delete;
        /// Deleted copy assignment operator
        noncopyable& operator=(const noncopyable&) = delete;
};

}
