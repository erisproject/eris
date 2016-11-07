#pragma once
#include <eris/Agent.hpp>
#include <eris/Bundle.hpp>
#include <eris/agent/ClearingAgent.hpp>

namespace eris { namespace agent {

/** Deprecated.  This class used to add an assets bundle to agents, but Agent now includes a public
 * asset bundle directly.  This class is provided only for backwards compatibility, returning access
 * to the base Agent bundle through an assets() method.  This, unfortunately, masks the base Agent
 * `assets` property, meaning it is not possible to mix AssetAgent and Agent asset access styles.
 *
 * To convert from AssetAgent to Agent, simply change the inheriting class and replace all
 * `->assets()` calls with an `->assets` member access.
 */
class [[deprecated("AssetAgent is deprecated; Agent now provides an .assets attribute")]] AssetAgent : public virtual Agent {
public:
    /// Returns a Bundle reference to the agent's current assets.
    Bundle& assets() { return Agent::assets; }

    /// `const` access to the agent's current assets.
    const Bundle& assets() const { return Agent::assets; }
};

/** This is a deprecated extension to AssetAgent that adds automatic assets clearing at the end of
 * every period (via an interopt::Advance implementation).  This class is simply an empty class that
 * inherits from both AssetAgent and ClearingAgent, provided for backwards compatibility.
 *
 * New code should use ClearingAgent instead.
 */
class AssetAgentClearing : public AssetAgent, public ClearingAgent {};

} }
