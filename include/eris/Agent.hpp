#pragma once
#include <eris/Bundle.hpp>
#include <eris/Member.hpp>
#include <utility>

namespace eris {

/** Namespace for Agent subclasses that are neither consumers nor firms. */
namespace agent {}

/** Base class for Agent objects. */
class Agent : public Member {
    public:
        /// Returns a Bundle reference to the agent's current assets.
        virtual Bundle& assets() noexcept { return assets_; }

        /// `const` access to the agent's current assets.
        virtual const Bundle& assets() const noexcept { return assets_; }

        /** Called when advancing a period.  Subclasses are intended to override (or enhance) as
         * required.  This could, for example, reset costs, reset costs, discard perishable output,
         * depreciate capital, etc.
         *
         * By default this clears the agent's assets bundle.
         */
        virtual void advance() { assets_.clear(); }

        /** Returns a hint for Simulation::run() as to whether the agent's advance() method should
         * be prescheduled across threads (if true) or put into a common queue processed across all
         * threads.  This is only called and used when Simualation's hybrid threading model is
         * active.  The default returns true; Agent subclasses with advance() methods requiring
         * substantial CPU time should override this method to return false.
         *
         * \sa Simulation::threadModel()
         */
        virtual bool preallocateAdvance() const { return true; }

    protected:

        /// Returns a SharedMember<Member> wrapped around the current object
        SharedMember<Member> sharedSelf() const override { return simAgent<Member>(id()); }

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
