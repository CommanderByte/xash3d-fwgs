#include "engine/server/server_timeout_policy.hpp"

#include "engine/server/client_session_slots.hpp"

namespace xash
{
namespace engine
{
namespace server
{

ServerTimeoutClientPlan BuildServerTimeoutClientPlan(
	const ServerTimeoutClientRequest &request)
{
	ServerTimeoutClientPlan plan = {};
	plan.activePlayer =
		ClientSessionSlotIsConnected(request.state) &&
		request.hasEntity &&
		!request.entitySpectator &&
		!request.entityFakeClient;

	if (request.fakeClient)
		return plan;

	if (request.state == kClientSessionSlotZombie)
	{
		plan.action = ServerTimeoutClientAction::FreeZombie;
		return plan;
	}

	if (request.localAddress)
		return plan;

	if ((request.state == kClientSessionSlotConnected ||
		request.state == kClientSessionSlotSpawning) &&
		request.connectionStarted < request.connectedDropPoint)
	{
		plan.action = ServerTimeoutClientAction::DropConnected;
		plan.ban = request.banConnectedTimeout;
		return plan;
	}

	if (request.state == kClientSessionSlotSpawned &&
		request.lastReceived < request.spawnedDropPoint)
	{
		plan.action = ServerTimeoutClientAction::DropSpawned;
		return plan;
	}

	return plan;
}

bool ShouldReleaseServerPauseForTimeouts(
	int maxClients,
	bool paused,
	int activePlayers)
{
	return maxClients > 1 && paused && activePlayers == 0;
}

}
}
}
