#ifndef XASH_ENGINE_SERVER_RESOURCE_ADAPTER_SHARED_HPP
#define XASH_ENGINE_SERVER_RESOURCE_ADAPTER_SHARED_HPP

#include "custom.h"
#include "engine/server/resources/resource_identity.hpp"
#include "engine/server/messaging/server_customization_message.hpp"
#include "engine/server/messaging/server_resource_message.hpp"

#include <cstddef>
#include <string>
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

struct LegacyResourceDescriptorFields
{
	resourcetype_t type;
	const char *name;
	int index;
	int downloadSize;
	unsigned char flags;
};

ResourceDescriptor ToModernResourceDescriptor(const resource_t *resource);
std::vector<ResourceDescriptor> BuildResourceDescriptorSnapshot(
	const resource_t *resources,
	int resourceCount);
std::vector<int> BuildCheckedResourceIndexSnapshot(
	const resource_t *resources,
	int resourceCount);
LegacyResourceDescriptorFields ToLegacyResourceDescriptorFields(
	const ResourceDescriptor &resource,
	resourcetype_t fallback = t_generic);

ResourceMessageRow ToModernResourceMessageRow(const resource_t *resource);
CustomizationMessage ToModernCustomizationMessage(
	const resource_t *resource,
	int playerNumber);
void CopyStringToLegacyBuffer(
	char *dst,
	std::size_t capacity,
	const std::string &src);

}
}
}
}

#endif
