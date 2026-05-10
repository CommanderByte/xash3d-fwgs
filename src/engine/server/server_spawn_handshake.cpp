#include "engine/server/server_spawn_handshake.hpp"

#include <cstdio>
#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

void WriteByte(
	xash::engine::network::NetworkBitBuffer &buffer,
	unsigned int value)
{
	buffer.writeUnsigned(static_cast<std::uint8_t>(value), 8);
}

void WriteWord(
	xash::engine::network::NetworkBitBuffer &buffer,
	unsigned int value)
{
	buffer.writeUnsigned(static_cast<std::uint16_t>(value), 16);
}

void WriteLong(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint32_t value)
{
	buffer.writeUnsigned(value, 32);
}

void WriteChar(
	xash::engine::network::NetworkBitBuffer &buffer,
	int value)
{
	buffer.writeSigned(static_cast<std::int8_t>(value), 8);
}

void WriteString(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		WriteByte(buffer, static_cast<unsigned char>(value[i]));
}

}

bool ShouldEmitServerdataPrint(bool developerMode, int maxClients)
{
	return developerMode || maxClients > 1;
}

int FormatServerdataPrintText(
	char *out,
	std::size_t outSize,
	int buildNumber,
	int progsCrc,
	int spawnCount)
{
	if (!out || outSize == 0)
		return 0;

	const int written = std::snprintf(
		out,
		outSize,
		"\n^3BUILD %d SERVER (%i CRC)\nServer #%i\n",
		buildNumber,
		progsCrc,
		spawnCount);

	out[outSize - 1] = '\0';
	return written;
}

void WriteServerdataPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ServerdataPayload &payload)
{
	WriteLong(buffer, static_cast<std::uint32_t>(payload.protocolVersion));
	WriteLong(buffer, static_cast<std::uint32_t>(payload.spawnCount));
	WriteLong(buffer, payload.worldMapCrc);
	WriteByte(buffer, static_cast<unsigned int>(payload.clientIndex));
	WriteByte(buffer, static_cast<unsigned int>(payload.maxClients));
	WriteWord(buffer, static_cast<unsigned int>(payload.maxEdicts));
	WriteWord(buffer, static_cast<unsigned int>(payload.maxModels));
	WriteString(buffer, payload.mapName);
	WriteString(buffer, payload.mapMessage);
	buffer.writeOneBit(payload.background ? 1 : 0);
	WriteString(buffer, payload.gameFolder);
	WriteLong(buffer, payload.hostFeatures);

	for (int i = 0; i < kServerdataHullComponentCount; ++i)
	{
		WriteChar(buffer, payload.playerMins[i]);
		WriteChar(buffer, payload.playerMaxs[i]);
	}
}

void WriteServerdataMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ServerdataPayload &payload)
{
	WriteByte(buffer, kServerdataCommand);
	WriteServerdataPayload(buffer, payload);
}

void WriteSignonNumberPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber)
{
	WriteByte(buffer, static_cast<unsigned int>(signonNumber));
}

void WriteSignonNumberMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber)
{
	WriteByte(buffer, kServerdataSignonNumberCommand);
	WriteSignonNumberPayload(buffer, signonNumber);
}

SpawnCommandAction BuildNewCommandAction(bool clientConnected)
{
	return clientConnected
		? SpawnCommandAction::SendServerdata
		: SpawnCommandAction::Reject;
}

SpawnCommandAction BuildSpawnCommandAction(
	bool clientConnected,
	int requestedSpawnCount,
	int currentSpawnCount)
{
	if (!clientConnected)
		return SpawnCommandAction::Reject;

	if (requestedSpawnCount != currentSpawnCount)
		return SpawnCommandAction::ResendNew;

	return SpawnCommandAction::PutClientInServer;
}

SpawnCommandAction BuildBeginCommandAction(bool clientSpawning)
{
	return clientSpawning
		? SpawnCommandAction::MarkSpawned
		: SpawnCommandAction::Reject;
}

int BuildSpawnResendFlags()
{
	return kSpawnHandshakeResendUserinfoFlag |
		kSpawnHandshakeResendMovevarsFlag;
}

bool ShouldSendSpawnSignon(bool fakeClient)
{
	return !fakeClient;
}

SpawnOverflowAction BuildSpawnSignonOverflowAction(
	bool overflowed,
	int maxClients)
{
	if (!overflowed)
		return SpawnOverflowAction::None;

	return maxClients == 1
		? SpawnOverflowAction::HostError
		: SpawnOverflowAction::DropClient;
}

}
}
}
