#pragma once
#include <eris/Bundle.hpp>
#include <eris/Member.hpp>

namespace eris {

/** Base class for Agent objects. */
class Agent : public Member {
    public:
        /** Returns a BundleNegative reference to the agent's current assets.  Note that the assets
         * variable may actually be a Bundle, rather than BundleNegative.  The default
         * implementation returns a Bundle, though subclasses could override this.
         */
        virtual BundleNegative& assets() noexcept { return assets_; }

        /// `const` access to the agent's current assets.
        virtual const BundleNegative& assets() const noexcept { return assets_; }

        /** Returns a Bundle reference to the agent's current assets.  This is simply a wrapper
         * around assets() plus a recast to a Bundle, and will fail if the BundleNegative returned
         * by assets() is not actually a Bundle.
         *
         * \throws std::bad_cast if the assets() BundleNegative cannot be cast to a Bundle.
         */
        Bundle& assetsB() { return dynamic_cast<Bundle&>(assets()); }

        /// `const` version of assetsB()
        const Bundle& assetsB() const { return dynamic_cast<const Bundle&>(assets()); }

    private:
        /** The current set of resources.  For a consumer, this could be the things to consume; for
         * a producer, this could be a stock of resources.
         *
         * Subclasses don't have to use this at all, but could, for example, record surplus
         * production here (for example, when a firm supplies 2 units of A for each unit of B, but
         * is instructed to supply 3 units of each (thus resulting in 3 surplus units of A).
         *
         * Since both firms and consumers typically have some sort of assets, this is provided here.
         */
        Bundle assets_;
};

}
