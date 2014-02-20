#pragma once
#include <iostream>

#ifndef ERIS_DEBUG
#define ERIS_DEBUG 0
#endif

#ifdef ERIS_DEBUG
// We need to screw around with __FILE__ to chop off everything before the last 'eris/' in it (just
// to make debugging a bit nicer)
namespace eris {
inline const char* _DEBUG__FILE__(const char *f) {
    std::string file(f);
    auto e = file.rfind("/eris/");
    return e == std::string::npos ? f : file.substr(e+1).c_str();
}
}
#define _eris__FILE__ eris::_DEBUG__FILE__(__FILE__)
#else
#define _eris__FILE__ __FILE__
#endif

/** Debugging macro.  DEBUGF(format, args) formats like printf and sends the output to / stderr,
 * prepended with the file/line number/function, and appended with a newline.
 */
#define DEBUGF(format, ...) do { if (ERIS_DEBUG) fprintf(stderr, "%s:%d:%s(): " format "\n", _eris__FILE__, __LINE__, __func__, ##__VA_ARGS__); } while (0)

/** Debugging macro.  DEBUG(a << b << c); sends a << b << c into std::cerr when debugging is
 * enabled, and does nothing otherwise.  The debugging output has the file/line/function prepended,
 * and a newline appended.
 */
#define DEBUG(stuff) do { if (ERIS_DEBUG) std::cerr << _eris__FILE__ << ":" << __LINE__ << ":" << __func__ << "(): " << stuff << "\n"; } while (0)

/** Debugging macro for a single variable.  DEBUGVAR(x) is just like DEBUG("x = " << x) */
#define DEBUGVAR(x) DEBUG(#x " = " << x)
