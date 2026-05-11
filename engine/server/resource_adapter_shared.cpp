#include "resource_adapter_shared.hpp"

#include "engine/server/resource_transfer_manifest.hpp"

#include <cstddef>
#include <cstdio>

namespace xash
{
namespace engine
{
namespace server
{
namespace adapter
{

ResourceType ToModernResourceType(resourcetype_t type)
{
	switch (type)
	{
	case t_sound:
		return ResourceType::Sound;
	case t_skin:
		return ResourceType::Skin;
	case t_model:
		return ResourceType::Model;
	case t_decal:
		return ResourceType::Decal;
	case t_generic:
		return ResourceType::Generic;
	case t_eventscript:
		return ResourceType::EventScript;
	case t_world:
		return ResourceType::World;
	default:
		return ResourceType::Unknown;
	}
}

resourcetype_t ToLegacyResourceType(ResourceType type, resourcetype_t fallback)
{
	switch (type)
	{
	case ResourceType::Sound:
		return t_sound;
	case ResourceType::Skin:
		return t_skin;
	case ResourceType::Model:
		return t_model;
	case ResourceType::Decal:
		return t_decal;
	case ResourceType::Generic:
		return t_generic;
	case ResourceType::EventScript:
		return t_eventscript;
	case ResourceType::World:
		return t_world;
	case ResourceType::Unknown:
	default:
		return fallback;
	}
}

ResourceDescriptor ToModernResourceDescriptor(const resource_t *resource)
{
	ResourceDescriptor modern = {};
	modern.type = ResourceType::Unknown;

	if (!resource)
		return modern;

	modern.name = resource->szFileName;
	modern.type = ToModernResourceType(resource->type);
	modern.index = resource->nIndex;
	modern.downloadSize = resource->nDownloadSize;
	modern.flags = resource->ucFlags;
	modern.md5Hash = resource->rgucMD5_hash;
	return modern;
}

std::vector<ResourceDescriptor> BuildResourceDescriptorSnapshot(
	const resource_t *resources,
	int resourceCount)
{
	std::vector<ResourceDescriptor> snapshot;

	if (!resources || resourceCount <= 0)
		return snapshot;

	snapshot.reserve(static_cast<std::size_t>(resourceCount));

	for (int i = 0; i < resourceCount; ++i)
		snapshot.push_back(ToModernResourceDescriptor(&resources[i]));

	return snapshot;
}

std::vector<int> BuildCheckedResourceIndexSnapshot(
	const resource_t *resources,
	int resourceCount)
{
	std::vector<int> indexes;

	if (!resources || resourceCount <= 0)
		return indexes;

	indexes.reserve(static_cast<std::size_t>(resourceCount));

	for (int i = 0; i < resourceCount; ++i)
	{
		if ((resources[i].ucFlags & RES_CHECKFILE) != 0)
			indexes.push_back(i);
	}

	return indexes;
}

LegacyResourceDescriptorFields ToLegacyResourceDescriptorFields(
	const ResourceDescriptor &resource,
	resourcetype_t fallback)
{
	LegacyResourceDescriptorFields fields = {};
	fields.type = ToLegacyResourceType(resource.type, fallback);
	fields.name = resource.name;
	fields.index = resource.index;
	fields.downloadSize = resource.downloadSize;
	fields.flags = static_cast<unsigned char>(resource.flags);
	return fields;
}

ResourceMessageRow ToModernResourceMessageRow(const resource_t *resource)
{
	const ResourceDescriptor descriptor = ToModernResourceDescriptor(resource);
	return BuildResourceTransferMessageRow(
		descriptor,
		resource ? resource->rguc_reserved : nullptr);
}

CustomizationMessage ToModernCustomizationMessage(
	const resource_t *resource,
	int playerNumber)
{
	const ResourceDescriptor descriptor = ToModernResourceDescriptor(resource);

	CustomizationMessage message = {};
	message.playerNumber = playerNumber;
	message.type = descriptor.type;
	message.name = descriptor.name;
	message.index = descriptor.index;
	message.downloadSize = descriptor.downloadSize;
	message.flags = descriptor.flags;
	message.md5Hash = descriptor.md5Hash;
	return message;
}

void CopyStringToLegacyBuffer(
	char *dst,
	std::size_t capacity,
	const std::string &src)
{
	if (!dst || capacity == 0)
		return;

	std::snprintf(dst, capacity, "%s", src.c_str());
}

}
}
}
}
