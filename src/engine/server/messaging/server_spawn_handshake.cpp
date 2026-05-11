#include "engine/server/messaging/server_spawn_handshake.hpp"
#include "engine/server/messaging/server_message_envelope.hpp"

#include <cstdio>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

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
	WriteServerMessageByte(buffer, static_cast<unsigned int>(payload.clientIndex));
	WriteServerMessageByte(buffer, static_cast<unsigned int>(payload.maxClients));
	WriteWord(buffer, static_cast<unsigned int>(payload.maxEdicts));
	WriteWord(buffer, static_cast<unsigned int>(payload.maxModels));
	WriteServerMessageString(buffer, payload.mapName);
	WriteServerMessageString(buffer, payload.mapMessage);
	buffer.writeOneBit(payload.background ? 1 : 0);
	WriteServerMessageString(buffer, payload.gameFolder);
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
	WriteServerMessageCommand(buffer, kServerdataCommand);
	WriteServerdataPayload(buffer, payload);
}

void WriteSignonNumberPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber)
{
	WriteServerMessageByte(buffer, static_cast<unsigned int>(signonNumber));
}

void WriteSignonNumberMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	int signonNumber)
{
	WriteServerMessageCommand(buffer, kServerdataSignonNumberCommand);
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
