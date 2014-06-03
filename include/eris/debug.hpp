#pragma once
#include <iostream>

#ifdef ERIS_DEBUG
#define ERIS_DEBUG_BOOL true
#else
#define ERIS_DEBUG_BOOL false
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

/** Debugging macro.  ERIS_DBGF(format, args) formats like printf and sends the output to stderr,
 * prepended with the file/line number/function, and appended with a newline.
 *
 * Does nothing unless compiled with `-DERIS_DEBUG`.
 */
#define ERIS_DBGF(format, ...) do { if (ERIS_DEBUG_BOOL) fprintf(stderr, "%s:%d:%s(): " format "\n", _eris__FILE__, __LINE__, __func__, ##__VA_ARGS__); std::cerr << std::flush; } while (0)

/** Debugging macro.  DEBUG(a << b << c); sends a << b << c into std::cerr when debugging is
 * enabled, and does nothing otherwise.  The debugging output has the file/line/function prepended,
 * and a newline appended.
 *
 * Does nothing unless compiled with `-DERIS_DEBUG`.
 */
#define ERIS_DBG(stuff) do { if (ERIS_DEBUG_BOOL) std::cerr << _eris__FILE__ << ":" << __LINE__ << ":" << __func__ << "(): " << stuff << "\n" << std::flush; } while (0)

/** Debugging macro for a single variable.  DEBUGVAR(x) is an alias for DEBUG("x = " << x) */
#define ERIS_DBGVAR(x) ERIS_DBG(#x " = " << x)
