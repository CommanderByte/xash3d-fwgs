#include "engine/server/shared/server_lifecycle_limits.hpp"

#include <limits>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

int ClampIntProduct(long long value)
{
	if (value <= 0)
		return 0;

	if (value > std::numeric_limits<int>::max())
		return std::numeric_limits<int>::max();

	return static_cast<int>(value);
}

int Clamp(int minimum, int value, int maximum)
{
	if (value < minimum)
		return minimum;

	if (value > maximum)
		return maximum;

	return value;
}

}

int ClampServerMaxClients(int requestedMaxClients, bool dedicatedServer)
{
	const int minimum = dedicatedServer
		? kServerDedicatedMinimumClients
		: kServerListenMinimumClients;

	return Clamp(minimum, requestedMaxClients, kServerMaxClients);
}

bool ServerUsesMultiplayerRules(int maxClients)
{
	return maxClients > 1;
}

int SelectServerUpdateBackup(int maxClients)
{
	return ServerUsesMultiplayerRules(maxClients)
		? kServerMultiplayerUpdateBackup
		: kServerSingleplayerUpdateBackup;
}

int BuildServerClientEntityCount(
	int maxClients,
	int updateBackup,
	int packetEntitiesPerFrame)
{
	if (maxClients <= 0 || updateBackup <= 0 || packetEntitiesPerFrame <= 0)
		return 0;

	return ClampIntProduct(
		static_cast<long long>(maxClients) *
		static_cast<long long>(updateBackup) *
		static_cast<long long>(packetEntitiesPerFrame));
}

int BuildServerGameEntityCount(int maxClients)
{
	if (maxClients < 0)
		return 1;

	if (maxClients >= std::numeric_limits<int>::max())
		return std::numeric_limits<int>::max();

	return maxClients + 1;
}

ServerClientCapacityPlan BuildServerClientCapacityPlan(
	int requestedMaxClients,
	bool dedicatedServer)
{
	ServerClientCapacityPlan plan = {};
	plan.maxClients = ClampServerMaxClients(
		requestedMaxClients,
		dedicatedServer);
	plan.multiplayer = ServerUsesMultiplayerRules(plan.maxClients);
	plan.updateBackup = SelectServerUpdateBackup(plan.maxClients);
	plan.updateMask = ServerUpdateMask(plan.updateBackup);
	plan.packetEntitiesPerFrame = kServerPacketEntitiesPerFrame;
	plan.clientEntityCount = BuildServerClientEntityCount(
		plan.maxClients,
		plan.updateBackup,
		plan.packetEntitiesPerFrame);
	plan.gameEntityCount = BuildServerGameEntityCount(plan.maxClients);
	return plan;
}

int BuildServerSpawnSettlingFrameCount(bool runPhysics, int maxClients)
{
	if (!runPhysics)
		return kServerNoPhysicsSpawnSettlingFrames;

	return ServerUsesMultiplayerRules(maxClients)
		? kServerMultiplayerSpawnSettlingFrames
		: kServerSingleplayerSpawnSettlingFrames;
}

double BuildServerSpawnSettlingFrameTime(bool runPhysics)
{
	return runPhysics
		? kServerSpawnTimeSeconds
		: kServerNoPhysicsSpawnFrameTime;
}

}
}
}
