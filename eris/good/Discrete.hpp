#pragma once
#include <eris/Good.hpp>

namespace eris { namespace good {

/** Good extension that can be used to tag a good as discrete, that is, a good that comes in fixed
 * increments (that is, it extends Good by overriding atom() to return a constant).  Note that this
 * is (currently) not enforced within eris, but is left to the caller to handle.
 */
class Discrete : public Good {
public:
    /// Constructor; takes a good name (defaults to blank) and an atom size (defaults to 1.0)
    explicit Discrete(std::string name = "", double atom = 1.0) : Good(name), atom_{atom} {}

    /// Returns the discrete atom size, as given during construction.
    virtual double atom() override { return atom_; }

private:
    const double atom_;
};

}}
