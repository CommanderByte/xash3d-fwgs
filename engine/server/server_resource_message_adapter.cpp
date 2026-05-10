#include "server_resource_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_resource_message.hpp"

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

xash::engine::server::ResourceMessageRow ToModernResourceRow(const resource_t *resource)
{
	xash::engine::server::ResourceMessageRow row = {};
	row.type = xash::engine::server::ResourceType::Unknown;

	if (!resource)
		return row;

	row.type = ToModernResourceType(resource->type);
	row.name = resource->szFileName;
	row.index = resource->nIndex;
	row.downloadSize = resource->nDownloadSize;
	row.flags = resource->ucFlags;
	row.md5Hash = resource->rgucMD5_hash;
	row.reservedData = resource->rguc_reserved;
	return row;
}

}

extern "C" sv_resource_message_write_result_t SV_ResourceMessage_WriteResource(
	const resource_t *resource,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));

	xash::engine::server::WriteResourceMessageRow(buffer, ToModernResourceRow(resource));

	sv_resource_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}
