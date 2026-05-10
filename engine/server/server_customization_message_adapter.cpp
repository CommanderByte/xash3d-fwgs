#include "server_customization_message_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_customization_message.hpp"

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

xash::engine::server::CustomizationMessage ToModernCustomizationMessage(
	const resource_t *resource,
	int playernum)
{
	xash::engine::server::CustomizationMessage message = {};
	message.playerNumber = playernum;
	message.type = xash::engine::server::ResourceType::Unknown;

	if (!resource)
		return message;

	message.type = ToModernResourceType(resource->type);
	message.name = resource->szFileName;
	message.index = resource->nIndex;
	message.downloadSize = resource->nDownloadSize;
	message.flags = resource->ucFlags;
	message.md5Hash = resource->rgucMD5_hash;
	return message;
}

}

extern "C" sv_customization_message_write_result_t SV_CustomizationMessage_WritePayload(
	const resource_t *resource,
	int playernum,
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));

	xash::engine::server::WriteCustomizationMessagePayload(
		buffer,
		ToModernCustomizationMessage(resource, playernum));

	sv_customization_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}
