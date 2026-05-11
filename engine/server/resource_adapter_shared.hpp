#ifndef XASH_ENGINE_SERVER_RESOURCE_ADAPTER_SHARED_HPP
#define XASH_ENGINE_SERVER_RESOURCE_ADAPTER_SHARED_HPP

#include "custom.h"
#include "engine/server/resource_identity.hpp"
#include "engine/server/server_customization_message.hpp"
#include "engine/server/server_resource_message.hpp"

#include <vector>

namespace xash
{
namespace engine
{
namespace server
{
namespace adapter
{

ResourceType ToModernResourceType(resourcetype_t type);
resourcetype_t ToLegacyResourceType(
	ResourceType type,
	resourcetype_t fallback = t_generic);

ResourceDescriptor ToModernResourceDescriptor(const resource_t *resource);
std::vector<ResourceDescriptor> BuildResourceDescriptorSnapshot(
	const resource_t *resources,
	int resourceCount);

ResourceMessageRow ToModernResourceMessageRow(const resource_t *resource);
CustomizationMessage ToModernCustomizationMessage(
	const resource_t *resource,
	int playerNumber);

}
}
}
}

#endif
