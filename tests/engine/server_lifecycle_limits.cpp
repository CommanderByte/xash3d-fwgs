#include <cmath>
#include <cstdlib>
#include <limits>

#include "engine/server/server_lifecycle_limits.hpp"

using namespace xash::engine::server;

namespace
{

static bool NearlyEqual(double left, double right)
{
	return std::fabs(left - right) <= 0.000001;
}

static bool TestMaxClientClamping()
{
	return ClampServerMaxClients(-8, false) == kServerListenMinimumClients &&
		ClampServerMaxClients(0, false) == kServerListenMinimumClients &&
		ClampServerMaxClients(1, false) == 1 &&
		ClampServerMaxClients(12, false) == 12 &&
		ClampServerMaxClients(64, false) == kServerMaxClients &&
		ClampServerMaxClients(1, true) == kServerDedicatedMinimumClients &&
		ClampServerMaxClients(4, true) == 4 &&
		ClampServerMaxClients(20, true) == 20 &&
		ClampServerMaxClients(64, true) == kServerMaxClients;
}

static bool TestUpdateBackupSelection()
{
	return !ServerUsesMultiplayerRules(1) &&
		ServerUsesMultiplayerRules(2) &&
		SelectServerUpdateBackup(1) == kServerSingleplayerUpdateBackup &&
		SelectServerUpdateBackup(2) == kServerMultiplayerUpdateBackup &&
		ServerUpdateMask(SelectServerUpdateBackup(2)) ==
			SelectServerUpdateBackup(2) - 1;
}

static bool TestClientEntityCounts()
{
	return BuildServerClientEntityCount(0, 64, 256) == 0 &&
		BuildServerClientEntityCount(1, 0, 256) == 0 &&
		BuildServerClientEntityCount(1, 64, 0) == 0 &&
		BuildServerClientEntityCount(2, 64, 256) == 32768 &&
		BuildServerClientEntityCount(
			kServerMaxClients,
			kServerMultiplayerUpdateBackup,
			kServerPacketEntitiesPerFrame) ==
			kServerMaxClients *
			kServerMultiplayerUpdateBackup *
			kServerPacketEntitiesPerFrame &&
		BuildServerClientEntityCount(
			std::numeric_limits<int>::max(),
			std::numeric_limits<int>::max(),
			std::numeric_limits<int>::max()) ==
			std::numeric_limits<int>::max();
}

static bool TestGameEntityCount()
{
	return BuildServerGameEntityCount(-2) == 1 &&
		BuildServerGameEntityCount(0) == 1 &&
		BuildServerGameEntityCount(1) == 2 &&
		BuildServerGameEntityCount(kServerMaxClients) ==
			kServerMaxClients + 1 &&
		BuildServerGameEntityCount(std::numeric_limits<int>::max()) ==
			std::numeric_limits<int>::max();
}

static bool TestCapacityPlan()
{
	const ServerClientCapacityPlan listen =
		BuildServerClientCapacityPlan(1, false);
	const ServerClientCapacityPlan dedicated =
		BuildServerClientCapacityPlan(1, true);

	return listen.maxClients == 1 &&
		!listen.multiplayer &&
		listen.updateBackup == kServerSingleplayerUpdateBackup &&
		listen.updateMask == kServerSingleplayerUpdateBackup - 1 &&
		listen.clientEntityCount ==
			kServerSingleplayerUpdateBackup *
			kServerPacketEntitiesPerFrame &&
		listen.gameEntityCount == 2 &&
		dedicated.maxClients == kServerDedicatedMinimumClients &&
		dedicated.multiplayer &&
		dedicated.updateBackup == kServerMultiplayerUpdateBackup &&
		dedicated.clientEntityCount ==
			kServerDedicatedMinimumClients *
			kServerMultiplayerUpdateBackup *
			kServerPacketEntitiesPerFrame &&
		dedicated.gameEntityCount ==
			kServerDedicatedMinimumClients + 1;
}

static bool TestSpawnSettlingPolicy()
{
	return BuildServerSpawnSettlingFrameCount(false, 1) ==
			kServerNoPhysicsSpawnSettlingFrames &&
		BuildServerSpawnSettlingFrameCount(true, 1) ==
			kServerSingleplayerSpawnSettlingFrames &&
		BuildServerSpawnSettlingFrameCount(true, 2) ==
			kServerMultiplayerSpawnSettlingFrames &&
		NearlyEqual(
			BuildServerSpawnSettlingFrameTime(false),
			kServerNoPhysicsSpawnFrameTime) &&
		NearlyEqual(
			BuildServerSpawnSettlingFrameTime(true),
			kServerSpawnTimeSeconds);
}

}

int main()
{
	if (!TestMaxClientClamping() ||
		!TestUpdateBackupSelection() ||
		!TestClientEntityCounts() ||
		!TestGameEntityCount() ||
		!TestCapacityPlan() ||
		!TestSpawnSettlingPolicy())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
