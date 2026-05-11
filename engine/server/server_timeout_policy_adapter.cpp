#include "server_timeout_policy_adapter.h"

#include "engine/server/server_timeout_policy.hpp"

static_assert(SV_TIMEOUT_CLIENT_ACTION_NONE ==
	static_cast<int>(xash::engine::server::ServerTimeoutClientAction::None),
	"timeout none action changed");
static_assert(SV_TIMEOUT_CLIENT_ACTION_FREE_ZOMBIE ==
	static_cast<int>(xash::engine::server::ServerTimeoutClientAction::FreeZombie),
	"timeout free-zombie action changed");
static_assert(SV_TIMEOUT_CLIENT_ACTION_DROP_CONNECTED ==
	static_cast<int>(xash::engine::server::ServerTimeoutClientAction::DropConnected),
	"timeout drop-connected action changed");
static_assert(SV_TIMEOUT_CLIENT_ACTION_DROP_SPAWNED ==
	static_cast<int>(xash::engine::server::ServerTimeoutClientAction::DropSpawned),
	"timeout drop-spawned action changed");

extern "C" sv_timeout_client_plan_t SV_Timeout_BuildClientPlan(
	const sv_timeout_client_request_t *request)
{
	xash::engine::server::ServerTimeoutClientRequest modern = {};

	if (request)
	{
		modern.state = request->state;
		modern.fakeClient = request->fake_client != 0;
		modern.hasEntity = request->has_entity != 0;
		modern.entitySpectator = request->entity_spectator != 0;
		modern.entityFakeClient = request->entity_fake_client != 0;
		modern.localAddress = request->local_address != 0;
		modern.connectionStarted = request->connection_started;
		modern.lastReceived = request->last_received;
		modern.connectedDropPoint = request->connected_drop_point;
		modern.spawnedDropPoint = request->spawned_drop_point;
		modern.banConnectedTimeout = request->ban_connected_timeout != 0;
	}

	const xash::engine::server::ServerTimeoutClientPlan plan =
		xash::engine::server::BuildServerTimeoutClientPlan(modern);

	sv_timeout_client_plan_t legacy = {};
	legacy.active_player = plan.activePlayer ? 1 : 0;
	legacy.action = static_cast<int>(plan.action);
	legacy.ban = plan.ban ? 1 : 0;
	return legacy;
}

extern "C" int SV_Timeout_ShouldReleasePause(
	int max_clients,
	int paused,
	int active_players)
{
	return xash::engine::server::ShouldReleaseServerPauseForTimeouts(
		max_clients,
		paused != 0,
		active_players) ? 1 : 0;
}
