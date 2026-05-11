#include <cstdlib>

#include "engine/server/client/client_session_slots.hpp"
#include "engine/server/client/server_timeout_policy.hpp"

using namespace xash::engine::server;

namespace
{

ServerTimeoutClientRequest DefaultRequest()
{
	ServerTimeoutClientRequest request = {};
	request.state = kClientSessionSlotSpawned;
	request.hasEntity = true;
	request.lastReceived = 20.0;
	request.connectionStarted = 20.0;
	request.connectedDropPoint = 10.0;
	request.spawnedDropPoint = 10.0;
	return request;
}

bool TestConnectedEntityCountsAsActivePlayer()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.activePlayer &&
		plan.action == ServerTimeoutClientAction::None;
}

bool TestSpectatorOrEntityFakeClientDoesNotCountActive()
{
	ServerTimeoutClientRequest spectator = DefaultRequest();
	spectator.entitySpectator = true;
	const ServerTimeoutClientRequest fake = [] {
		ServerTimeoutClientRequest request = DefaultRequest();
		request.entityFakeClient = true;
		return request;
	}();

	return !BuildServerTimeoutClientPlan(spectator).activePlayer &&
		!BuildServerTimeoutClientPlan(fake).activePlayer;
}

bool TestFakeClientSkipsTimeoutButCanStillCountIfEntityLooksActive()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.fakeClient = true;
	request.lastReceived = 1.0;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.activePlayer &&
		plan.action == ServerTimeoutClientAction::None;
}

bool TestZombieBecomesFree()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.state = kClientSessionSlotZombie;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return !plan.activePlayer &&
		plan.action == ServerTimeoutClientAction::FreeZombie;
}

bool TestConnectedTimeoutDropsAndCanBan()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.state = kClientSessionSlotConnected;
	request.connectionStarted = 5.0;
	request.connectedDropPoint = 10.0;
	request.banConnectedTimeout = true;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.activePlayer &&
		plan.action == ServerTimeoutClientAction::DropConnected &&
		plan.ban;
}

bool TestSpawningTimeoutUsesConnectedDropPoint()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.state = kClientSessionSlotSpawning;
	request.connectionStarted = 5.0;
	request.connectedDropPoint = 10.0;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.action == ServerTimeoutClientAction::DropConnected;
}

bool TestSpawnedTimeoutDropsWithoutBan()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.lastReceived = 5.0;
	request.spawnedDropPoint = 10.0;
	request.banConnectedTimeout = true;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.action == ServerTimeoutClientAction::DropSpawned &&
		!plan.ban;
}

bool TestLocalAddressDoesNotTimeout()
{
	ServerTimeoutClientRequest request = DefaultRequest();
	request.lastReceived = 5.0;
	request.spawnedDropPoint = 10.0;
	request.localAddress = true;

	const ServerTimeoutClientPlan plan =
		BuildServerTimeoutClientPlan(request);

	return plan.action == ServerTimeoutClientAction::None;
}

bool TestPauseReleaseRequiresMultiplayerPauseAndNoActivePlayers()
{
	return ShouldReleaseServerPauseForTimeouts(2, true, 0) &&
		!ShouldReleaseServerPauseForTimeouts(1, true, 0) &&
		!ShouldReleaseServerPauseForTimeouts(2, false, 0) &&
		!ShouldReleaseServerPauseForTimeouts(2, true, 1);
}

}

int main()
{
	if (!TestConnectedEntityCountsAsActivePlayer() ||
		!TestSpectatorOrEntityFakeClientDoesNotCountActive() ||
		!TestFakeClientSkipsTimeoutButCanStillCountIfEntityLooksActive() ||
		!TestZombieBecomesFree() ||
		!TestConnectedTimeoutDropsAndCanBan() ||
		!TestSpawningTimeoutUsesConnectedDropPoint() ||
		!TestSpawnedTimeoutDropsWithoutBan() ||
		!TestLocalAddressDoesNotTimeout() ||
		!TestPauseReleaseRequiresMultiplayerPauseAndNoActivePlayers())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
