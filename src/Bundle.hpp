#pragma once
#include <map>
#include "types.hpp"

// A Bundle is a map, of sorts, between good ids and quantities.
//
// It's a bit more specialized than an std::map, however, in that it returns a
// quantity of 0 for goods that are not in the bundle.  The mechanism for
// removing a good is thus assigning a quantity of 0 to that good.
//
// Accessing a quantity is done through the [] operation (e.g. bundle[gid]);
// setting requires a call to the set(gid, quantity) method.
//
// You can iterate through goods via the usual cbegin()/cend() pattern; these
// get passed straight through to the underlying std::map's methods.  (Note that begin()/end() are not exposed
//
// Bundle allows only non-negative quantities for goods, but the
// Bundle::Negative nested class is also provided which lifts this restriction.
//
// Note that these class does *not* enforce a Good's increment parameter; any
// class directly modifying a Bundle should thus ensure that the increment is
// respected.

class Bundle {
    public:
        double operator[] (eris_id_t gid) const;
        void set(eris_id_t gid, double quantity);
        std::map<eris_id_t, double>::const_iterator begin();
        std::map<eris_id_t, double>::const_iterator end();
        class Negative;
    private:
        std::map<eris_id_t, double> bundle;
};

class Bundle::Negative : public Bundle {
    void set(eris_id_t gid, double quantity);
};
