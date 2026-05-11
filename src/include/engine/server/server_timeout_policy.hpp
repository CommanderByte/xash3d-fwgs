#ifndef XASH_ENGINE_SERVER_TIMEOUT_POLICY_HPP
#define XASH_ENGINE_SERVER_TIMEOUT_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class ServerTimeoutClientAction
{
	None = 0,
	FreeZombie = 1,
	DropConnected = 2,
	DropSpawned = 3,
};

struct ServerTimeoutClientRequest
{
	int state;
	bool fakeClient;
	bool hasEntity;
	bool entitySpectator;
	bool entityFakeClient;
	bool localAddress;
	double connectionStarted;
	double lastReceived;
	double connectedDropPoint;
	double spawnedDropPoint;
	bool banConnectedTimeout;
};

struct ServerTimeoutClientPlan
{
	bool activePlayer;
	ServerTimeoutClientAction action;
	bool ban;
};

ServerTimeoutClientPlan BuildServerTimeoutClientPlan(
	const ServerTimeoutClientRequest &request);

bool ShouldReleaseServerPauseForTimeouts(
	int maxClients,
	bool paused,
	int activePlayers);

}
}
}

#endif
