#pragma once
#include <iostream>
#include <ctime>
#include <sstream>

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
// If debugging is not enabled, it doesn't matter: we just need something that will compile:
#define _eris__FILE__ ""
#endif

#define _eris_dbgf(prefix, format, ...) fprintf(stderr, "%s%s:%d:%s(): " format "\n", prefix, _eris__FILE__, __LINE__, __func__, ##__VA_ARGS__); std::cerr << std::flush
#define _eris_dbg(prefix, stuff) std::ostringstream s; s << prefix << _eris__FILE__ << ":" << __LINE__ << ":" << __func__ << "(): " << stuff << "\n"; std::cerr << s.str() << std::flush

/** Debugging macro.  ERIS_DBGF(format, args) formats like printf and sends the output to stderr,
 * prepended with the file/line number/function, and appended with a newline.
 *
 * Does nothing unless compiled with `-DERIS_DEBUG`.
 */
#define ERIS_DBGF(format, ...) do { if (ERIS_DEBUG_BOOL) { _eris_dbgf("", format, ##__VA_ARGS__); } } while (0)

/** Debugging macro.  DEBUG(a << b << c); sends a << b << c into std::cerr when debugging is
 * enabled, and does nothing otherwise.  The debugging output has the file/line/function prepended,
 * and a newline appended.
 *
 * Does nothing unless compiled with `-DERIS_DEBUG`.
 */
#define ERIS_DBG(stuff) do { if (ERIS_DEBUG_BOOL) { _eris_dbg("", stuff); } } while (0)

/** Debugging macro for a single variable.  DEBUGVAR(x) is an alias for DEBUG("x = " << (x)) */
#define ERIS_DBGVAR(x) ERIS_DBG(#x " = " << (x))

// stdc++-4.9 and earlier don't support the C++11 put_time, so this doesn't work:
//#define _eris_PREPEND_TIME if (ERIS_DEBUG_BOOL) { std::time_t t = std::time(nullptr); std::cerr << std::put_time(std::localtime(&t), "[%c] "); }
#define _eris_tstr std::time_t t = std::time(nullptr); char tstr[100]; std::strftime(tstr, sizeof(tstr), "[%c] ", std::localtime(&t))

/** Debugging macro just like `ERIS_DBGF` but also prefixes output with the current date and time. */
#define ERIS_TDBGF(format, ...) do { if (ERIS_DEBUG_BOOL) { _eris_tstr; _eris_dbgf(tstr, format, ##__VA_ARGS__); } } while (0)

/** Debugging macro just like `ERIS_DBG`, but also prefixes output with the current date and time. */
#define ERIS_TDBG(stuff) do { if (ERIS_DEBUG_BOOL) { _eris_tstr; _eris_dbg(tstr, stuff); } } while (0)

/** Debugging macro just like `ERIS_DBGVAR`, but also prefixes output with the current date and
 * time. */
#define ERIS_TDBGVAR(x) ERIS_TDBG(#x " = " << x)
