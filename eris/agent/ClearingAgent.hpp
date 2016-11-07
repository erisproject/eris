#pragma once
#include <eris/Agent.hpp>
#include <eris/Bundle.hpp>
#include <eris/Optimize.hpp>

namespace eris { namespace agent {

/** This class extends Agent by adding an interAdvance() optimizer that clears the agents assets
 * when transitioning from one period to the next.
 */
class ClearingAgent : public virtual Agent, public virtual interopt::Advance {
public:
    /** Called when advancing a period.  Subclasses are intended to override (or enhance) as
     * required.  This could, for example, reset costs, discard perishable output, depreciate
     * capital, etc.
     *
     * This method clears the agent's assets.
     */
    virtual void interAdvance() override { assets.clear(); }
};

} }
