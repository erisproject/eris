#pragma once
#include <eris/Member.hpp>

namespace eris {

/** Namespace for Agent subclasses that are neither consumers nor firms. */
namespace agent {}

/** Base class for Agent objects. This adds nothing to the Member base, but is used as a decorator
 * class for all agents.
 *
 * \sa eris::agent::AssetAgent for an Agent base that includes a default assets bundle
 */
class Agent : public Member {
    protected:
        /// Returns a SharedMember<Member> wrapped around the current object
        SharedMember<Member> sharedSelf() const override { return simAgent(id()); }

};

}
