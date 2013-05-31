#pragma once
#include <eris/Bundle.hpp>
#include <eris/Member.hpp>

namespace eris {

/* Base class for Agent objects. */
class Agent : public Member {
    public:
    protected:
        Agent() {}

        /** Accesses a reference to the agent's current assets.  The default implementation simply
         * returns the assets field, below, but subclasses could return some other reference if not
         * using the assets field.
         */
        virtual BundleNegative& assetsRef() { return assets; }

        /** The current set of resources.  For a consumer, this could be the things to consume; for
         * a producer, this could be a stock of resources.
         *
         * Subclasses don't have to use this at all, but could, for example, record surplus
         * production here (for example, when a firm supplies 2 units of A for each unit of B, but
         * is instructed to supply 3 units of each (thus resulting in 3 surplus units of A).
         *
         * Since both firms and consumers typically have some sort of assets, this is provided here.
         */
        Bundle assets;

        friend class Optimizer;
};

}
