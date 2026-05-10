#ifndef XASH_ENGINE_SERVER_SERVER_LIFECYCLE_LIMITS_HPP
#define XASH_ENGINE_SERVER_SERVER_LIFECYCLE_LIMITS_HPP

#include "engine/server/server_limits.hpp"

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kServerMaxClientBits = 5;
constexpr int kServerMaxClients = 1 << kServerMaxClientBits;

#if XASH_LOW_MEMORY == 2
constexpr int kServerMultiplayerUpdateBackup = 4;
constexpr int kServerSingleplayerUpdateBackup = 4;
constexpr int kServerPacketEntitiesPerFrame = 32;
#elif XASH_LOW_MEMORY == 1
constexpr int kServerMultiplayerUpdateBackup = 64;
constexpr int kServerSingleplayerUpdateBackup = 4;
constexpr int kServerPacketEntitiesPerFrame = 64;
#else
constexpr int kServerMultiplayerUpdateBackup = 64;
constexpr int kServerSingleplayerUpdateBackup = 16;
constexpr int kServerPacketEntitiesPerFrame = 256;
#endif

constexpr int kServerDedicatedMinimumClients = 4;
constexpr int kServerListenMinimumClients = 1;
constexpr double kServerNoPhysicsSpawnFrameTime = 0.001;
constexpr int kServerSingleplayerSpawnSettlingFrames = 2;
constexpr int kServerMultiplayerSpawnSettlingFrames = 8;
constexpr int kServerNoPhysicsSpawnSettlingFrames = 1;

struct ServerClientCapacityPlan
{
	int maxClients;
	int updateBackup;
	int updateMask;
	int packetEntitiesPerFrame;
	int clientEntityCount;
	int gameEntityCount;
	bool multiplayer;
};

int ClampServerMaxClients(int requestedMaxClients, bool dedicatedServer);
bool ServerUsesMultiplayerRules(int maxClients);
int SelectServerUpdateBackup(int maxClients);
int BuildServerClientEntityCount(
	int maxClients,
	int updateBackup,
	int packetEntitiesPerFrame = kServerPacketEntitiesPerFrame);
int BuildServerGameEntityCount(int maxClients);
ServerClientCapacityPlan BuildServerClientCapacityPlan(
	int requestedMaxClients,
	bool dedicatedServer);
int BuildServerSpawnSettlingFrameCount(bool runPhysics, int maxClients);
double BuildServerSpawnSettlingFrameTime(bool runPhysics);

}
}
}

#endif
