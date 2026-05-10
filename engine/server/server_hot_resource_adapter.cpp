#include "server_hot_resource_adapter.h"

#include "engine/server/server_hot_resource.hpp"

#include <cstdio>

namespace
{

xash::engine::server::ResourceType ToModernResourceType(resourcetype_t type)
{
	using xash::engine::server::ResourceType;

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

resourcetype_t ToLegacyResourceType(
	xash::engine::server::ResourceType type,
	resourcetype_t fallback)
{
	using xash::engine::server::ResourceType;

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

xash::engine::server::HotResourceRequest ToModernRequest(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags)
{
	xash::engine::server::HotResourceRequest request = {};
	request.type = ToModernResourceType(type);
	request.name = name;
	request.index = index;
	request.flags = flags;
	return request;
}

}

extern "C" sv_hot_resource_file_size_query_t SV_HotResource_BuildFileSizeQuery(
	const char *name,
	resourcetype_t type)
{
	const xash::engine::server::HotResourceFileSizeQuery modern =
		xash::engine::server::BuildHotResourceFileSizeQuery(
			ToModernRequest(name, type, 0, 0));

	sv_hot_resource_file_size_query_t legacy = {};
	legacy.should_announce = modern.shouldAnnounce ? 1 : 0;
	legacy.needs_file_size = modern.needsFileSize ? 1 : 0;

	if (!modern.path.empty())
	{
		std::snprintf(
			legacy.file_size_path,
			sizeof(legacy.file_size_path),
			"%s",
			modern.path.c_str());
	}

	return legacy;
}

extern "C" sv_hot_resource_entry_t SV_HotResource_BuildAnnouncement(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags,
	int probed_download_size)
{
	const xash::engine::server::HotResourceAnnouncement modern =
		xash::engine::server::BuildHotResourceAnnouncement(
			ToModernRequest(name, type, index, flags),
			probed_download_size);

	sv_hot_resource_entry_t legacy = {};
	legacy.should_announce = modern.shouldAnnounce ? 1 : 0;
	legacy.type = ToLegacyResourceType(modern.resource.type, type);
	legacy.name = modern.resource.name;
	legacy.index = modern.resource.index;
	legacy.download_size = modern.resource.downloadSize;
	legacy.flags = static_cast<unsigned char>(modern.resource.flags);
	return legacy;
}
