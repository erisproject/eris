#pragma once
#include <eris/Bundle.hpp>
#include <eris/Member.hpp>

namespace eris {

/** Base class for Agent objects. */
class Agent : public Member {
    public:
        /// Returns a Bundle reference to the agent's current assets.
        virtual Bundle& assets() noexcept { return assets_; }

        /// `const` access to the agent's current assets.
        virtual const Bundle& assets() const noexcept { return assets_; }

        /** Called when advancing a period.  Subclasses are intended to override (or enhance) as
         * required.  This could, for example, reset costs, reset costs, discard perishable output,
         * depreciate capital, etc.  By default clears the agent's assets.
         */
        virtual void advance() { assets_.clear(); }
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
