#include "server_static_messages_adapter.h"

#include "engine/server/messaging/server_static_messages.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_spawn_static_decision_t SV_StaticMessage_BuildSpawnStaticDecision(
	int index,
	int max_static_entities,
	int bytes_left)
{
	const xash::engine::server::SpawnStaticDecision modern =
		xash::engine::server::BuildSpawnStaticDecision(
			index,
			max_static_entities,
			bytes_left);

	sv_spawn_static_decision_t legacy = {};
	legacy.should_write = modern.shouldWrite ? 1 : 0;
	legacy.reason = static_cast<int>(modern.reason);
	return legacy;
}

extern "C" sv_static_message_write_result_t SV_StaticMessage_WriteBspDecalPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	float origin_x,
	float origin_y,
	float origin_z,
	int decal_index,
	int entity_index,
	int model_index,
	int flags,
	float scale,
	int large_coordinates)
{
	xash::engine::server::StaticDecalPayload payload = {};
	payload.origin[0] = origin_x;
	payload.origin[1] = origin_y;
	payload.origin[2] = origin_z;
	payload.decalIndex = decal_index;
	payload.entityIndex = entity_index;
	payload.modelIndex = model_index;
	payload.flags = flags;
	payload.scale = scale;
	payload.largeCoordinates = large_coordinates != 0;

	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteBspDecalPayload(buffer, payload);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_static_message_write_result_t>(buffer);
}
