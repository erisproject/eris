#pragma once
#include <eris/Agent.hpp>
#include <eris/Optimize.hpp>

namespace eris { namespace agent {

/** This is a simple extension of the base Agent class that adds an assets bundle to the agent
 * plus an interAdvance() method that clears the assets bundle in every period.
 */
class AssetAgent : public virtual Agent, public virtual interopt::Advance {
    public:
        /// Returns a Bundle reference to the agent's current assets.
        virtual Bundle& assets() noexcept { return assets_; }

        /// `const` access to the agent's current assets.
        virtual const Bundle& assets() const noexcept { return assets_; }

        /** Called when advancing a period.  Subclasses are intended to override (or enhance) as
         * required.  This could, for example, reset costs, discard perishable output, depreciate
         * capital, etc.
         *
         * By default this clears the agent's assets bundle.
         */
        virtual void interAdvance() override { assets_.clear(); }

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

} }
