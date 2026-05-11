#include "server_hot_resource_adapter.h"

#include "engine/server/resources/server_hot_resource.hpp"
#include "resource_adapter_shared.hpp"

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
		xash::engine::server::adapter::CopyStringToLegacyBuffer(
			legacy.file_size_path,
			sizeof(legacy.file_size_path),
			modern.path);
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
	const xash::engine::server::adapter::LegacyResourceDescriptorFields fields =
		xash::engine::server::adapter::ToLegacyResourceDescriptorFields(
			modern.resource,
			type);

	sv_hot_resource_entry_t legacy = {};
	legacy.should_announce = modern.shouldAnnounce ? 1 : 0;
	legacy.type = fields.type;
	legacy.name = fields.name;
	legacy.index = fields.index;
	legacy.download_size = fields.downloadSize;
	legacy.flags = fields.flags;
	return legacy;
}
