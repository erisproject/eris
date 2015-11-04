#include <eris/agent/AssetAgent.hpp>

namespace eris { namespace agent {

Bundle& AssetAgent::assets() noexcept { return assets_; }
const Bundle& AssetAgent::assets() const noexcept { return assets_; }

void AssetAgentClearing::interAdvance() { assets().clear(); }

} }
