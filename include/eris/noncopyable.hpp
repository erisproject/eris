#pragma once

namespace eris {

/** Simple utility class intended to be inherited (privately) for a class that should not be copied.
 */
class noncopyable {
    protected:
        noncopyable() = default;
        ~noncopyable() = default;
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
};

}
