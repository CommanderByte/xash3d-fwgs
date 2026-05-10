#include "server_lifecycle_limits_adapter.h"

#include "engine/server/server_lifecycle_limits.hpp"

extern "C" int SV_Lifecycle_ClampMaxClients(
	int requested_max_clients,
	int dedicated_server)
{
	return xash::engine::server::ClampServerMaxClients(
		requested_max_clients,
		dedicated_server != 0);
}

extern "C" int SV_Lifecycle_UsesMultiplayerRules(int max_clients)
{
	return xash::engine::server::ServerUsesMultiplayerRules(max_clients) ? 1 : 0;
}

extern "C" int SV_Lifecycle_SelectUpdateBackup(int max_clients)
{
	return xash::engine::server::SelectServerUpdateBackup(max_clients);
}

extern "C" int SV_Lifecycle_ClientEntityCount(
	int max_clients,
	int update_backup,
	int packet_entities_per_frame)
{
	return xash::engine::server::BuildServerClientEntityCount(
		max_clients,
		update_backup,
		packet_entities_per_frame);
}

extern "C" int SV_Lifecycle_DefaultClientEntityCount(
	int max_clients,
	int update_backup)
{
	return xash::engine::server::BuildServerClientEntityCount(
		max_clients,
		update_backup);
}

extern "C" int SV_Lifecycle_GameEntityCount(int max_clients)
{
	return xash::engine::server::BuildServerGameEntityCount(max_clients);
}

extern "C" int SV_Lifecycle_SpawnSettlingFrameCount(
	int run_physics,
	int max_clients)
{
	return xash::engine::server::BuildServerSpawnSettlingFrameCount(
		run_physics != 0,
		max_clients);
}

extern "C" double SV_Lifecycle_SpawnSettlingFrameTime(int run_physics)
{
	return xash::engine::server::BuildServerSpawnSettlingFrameTime(
		run_physics != 0);
}
