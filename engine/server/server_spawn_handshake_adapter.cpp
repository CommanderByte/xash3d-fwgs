#include "server_spawn_handshake_adapter.h"

#include "engine/server/messaging/server_spawn_handshake.hpp"
#include "server_message_adapter_shared.hpp"

namespace
{

enum sv_spawn_command_action_e ToLegacyAction(
	xash::engine::server::SpawnCommandAction action)
{
	return static_cast<enum sv_spawn_command_action_e>(action);
}

enum sv_spawn_overflow_action_e ToLegacyOverflowAction(
	xash::engine::server::SpawnOverflowAction action)
{
	return static_cast<enum sv_spawn_overflow_action_e>(action);
}

}

extern "C" int SV_Serverdata_ShouldEmitPrint(
	int developer_mode,
	int max_clients)
{
	return xash::engine::server::ShouldEmitServerdataPrint(
		developer_mode != 0,
		max_clients) ? 1 : 0;
}

extern "C" void SV_Serverdata_FormatPrintText(
	char *out,
	unsigned int out_size,
	int build_number,
	int progs_crc,
	int spawn_count)
{
	xash::engine::server::FormatServerdataPrintText(
		out,
		out_size,
		build_number,
		progs_crc,
		spawn_count);
}

extern "C" sv_spawn_handshake_write_result_t SV_Serverdata_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int protocol_version,
	int spawn_count,
	unsigned int world_map_crc,
	int client_index,
	int max_clients,
	int max_edicts,
	int max_models,
	const char *map_name,
	const char *map_message,
	int background,
	const char *game_folder,
	unsigned int host_features,
	const float *player_mins,
	const float *player_maxs,
	int hull_count)
{
	xash::engine::server::ServerdataPayload payload = {};
	payload.protocolVersion = protocol_version;
	payload.spawnCount = spawn_count;
	payload.worldMapCrc = world_map_crc;
	payload.clientIndex = client_index;
	payload.maxClients = max_clients;
	payload.maxEdicts = max_edicts;
	payload.maxModels = max_models;
	payload.mapName = map_name;
	payload.mapMessage = map_message;
	payload.background = background != 0;
	payload.gameFolder = game_folder;
	payload.hostFeatures = host_features;

	const int count = hull_count < xash::engine::server::kServerdataHullCount
		? hull_count
		: xash::engine::server::kServerdataHullCount;

	for (int hull = 0; hull < count; ++hull)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			const int index = hull * 3 + axis;
			payload.playerMins[index] = player_mins
				? static_cast<int>(player_mins[index])
				: 0;
			payload.playerMaxs[index] = player_maxs
				? static_cast<int>(player_maxs[index])
				: 0;
		}
	}

	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_spawn_handshake_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteServerdataPayload,
			payload);
}

extern "C" sv_spawn_handshake_write_result_t SV_SpawnHandshake_WriteSignonNumberMessage(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int signon_number)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_spawn_handshake_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteSignonNumberMessage,
			signon_number);
}

extern "C" enum sv_spawn_command_action_e SV_SpawnHandshake_BuildNewCommandAction(
	int client_connected)
{
	return ToLegacyAction(xash::engine::server::BuildNewCommandAction(
		client_connected != 0));
}

extern "C" enum sv_spawn_command_action_e SV_SpawnHandshake_BuildSpawnCommandAction(
	int client_connected,
	int requested_spawn_count,
	int current_spawn_count)
{
	return ToLegacyAction(xash::engine::server::BuildSpawnCommandAction(
		client_connected != 0,
		requested_spawn_count,
		current_spawn_count));
}

extern "C" enum sv_spawn_command_action_e SV_SpawnHandshake_BuildBeginCommandAction(
	int client_spawning)
{
	return ToLegacyAction(xash::engine::server::BuildBeginCommandAction(
		client_spawning != 0));
}

extern "C" int SV_SpawnHandshake_BuildResendFlags(void)
{
	return xash::engine::server::BuildSpawnResendFlags();
}

extern "C" int SV_SpawnHandshake_ShouldSendSignon(int fake_client)
{
	return xash::engine::server::ShouldSendSpawnSignon(fake_client != 0) ? 1 : 0;
}

extern "C" enum sv_spawn_overflow_action_e SV_SpawnHandshake_BuildSignonOverflowAction(
	int overflowed,
	int max_clients)
{
	return ToLegacyOverflowAction(
		xash::engine::server::BuildSpawnSignonOverflowAction(
			overflowed != 0,
			max_clients));
}
