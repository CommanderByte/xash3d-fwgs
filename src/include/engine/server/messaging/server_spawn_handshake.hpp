#ifndef XASH_ENGINE_SERVER_SERVER_SPAWN_HANDSHAKE_HPP
#define XASH_ENGINE_SERVER_SERVER_SPAWN_HANDSHAKE_HPP

#include "engine/network/network_buffer.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kServerdataPrintCommand = 8;
constexpr std::uint8_t kServerdataCommand = 11;
constexpr std::uint8_t kServerdataSignonNumberCommand = 25;
constexpr int kServerdataHullCount = 4;
constexpr int kServerdataHullComponentCount = kServerdataHullCount * 3;
constexpr int kSpawnHandshakeResendUserinfoFlag = 1 << 0;
constexpr int kSpawnHandshakeResendMovevarsFlag = 1 << 1;
constexpr int kSpawnHandshakeDefaultSignonNumber = 1;

struct ServerdataPayload
{
	int protocolVersion;
	int spawnCount;
	std::uint32_t worldMapCrc;
	int clientIndex;
	int maxClients;
	int maxEdicts;
	int maxModels;
	const char *mapName;
	const char *mapMessage;
	bool background;
	const char *gameFolder;
	std::uint32_t hostFeatures;
	int playerMins[kServerdataHullComponentCount];
	int playerMaxs[kServerdataHullComponentCount];
};

enum class SpawnCommandAction
{
	Reject = 0,
	SendServerdata = 1,
	ResendNew = 2,
	PutClientInServer = 3,
	MarkSpawned = 4
};

enum class SpawnOverflowAction
{
	None = 0,
	HostError = 1,
	DropClient = 2
};

bool ShouldEmitServerdataPrint(bool developerMode, int maxClients);
int FormatServerdataPrintText(
	char *out,
	std::size_t outSize,
	int buildNumber,
	int progsCrc,
	int spawnCount);

void WriteServerdataPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ServerdataPayload &payload);
void WriteServerdataMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ServerdataPayload &payload);

void WriteSignonNumberPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber);
void WriteSignonNumberMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber);

SpawnCommandAction BuildNewCommandAction(bool clientConnected);
SpawnCommandAction BuildSpawnCommandAction(
	bool clientConnected,
	int requestedSpawnCount,
	int currentSpawnCount);
SpawnCommandAction BuildBeginCommandAction(bool clientSpawning);
int BuildSpawnResendFlags();
bool ShouldSendSpawnSignon(bool fakeClient);
SpawnOverflowAction BuildSpawnSignonOverflowAction(
	bool overflowed,
	int maxClients);

}
}
}

#endif
