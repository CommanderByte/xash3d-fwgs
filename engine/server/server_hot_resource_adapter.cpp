#include "server_hot_resource_adapter.h"

#include "engine/server/server_hot_resource.hpp"
#include "resource_adapter_shared.hpp"

#include <cstdio>

namespace
{

xash::engine::server::HotResourceRequest ToModernRequest(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags)
{
	xash::engine::server::HotResourceRequest request = {};
	request.type = xash::engine::server::adapter::ToModernResourceType(type);
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
	legacy.type = xash::engine::server::adapter::ToLegacyResourceType(
		modern.resource.type,
		type);
	legacy.name = modern.resource.name;
	legacy.index = modern.resource.index;
	legacy.download_size = modern.resource.downloadSize;
	legacy.flags = static_cast<unsigned char>(modern.resource.flags);
	return legacy;
}
