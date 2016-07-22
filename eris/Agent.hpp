#pragma once
#include <eris/Member.hpp>
#include <eris/Bundle.hpp>

namespace eris {

/** Namespace for Agent subclasses that are neither consumers nor firms. */
namespace agent {}

/** Base class for Agent objects. This adds only a public `assets` bundle to the Member base; it is
 * otherwise mainly a decorator class for all agents.
 */
class Agent : public Member {
public:
    /** The agent's assets.
     * For a consumer, this assets bundle could be the things to consume and/or income; for a
     * producer, this could contain a stock of resources and profits.
     *
     * \sa eris::agent::ClearingAgent for a version that automatically clears the assets bundle
     * at the beginning of each period.
     */
    Bundle assets;
protected:
    /// Returns a SharedMember<Member> wrapped around the current object
    SharedMember<Member> sharedSelf() const override;

};

}
