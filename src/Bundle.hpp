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
// Bundle is actually a specialization of BundleNegative, also in this file.  Bundle allows only
// non-negative quantities, while BundleNegative allows positive and negative quantities for the
// handled goods.
// Bundle allows only non-negative quantities for goods, but the
// BundleNegative nested class is also provided which lifts this restriction.
//
// Note that these class does *not* enforce a Good's increment parameter; any
// class directly modifying a Bundle should thus ensure that the increment is
// respected.

class Bundle;
class BundleNegative {
    private:
        std::map<eris_id_t, double> bundle;
    public:
        virtual void set(eris_id_t gid, double quantity);
        virtual double operator[] (eris_id_t gid) const;
        virtual std::map<eris_id_t, double>::const_iterator begin();
        virtual std::map<eris_id_t, double>::const_iterator end();

        friend class Bundle;
};

class Bundle : public BundleNegative {
    public:
        virtual void set(eris_id_t gid, double quantity);
};
