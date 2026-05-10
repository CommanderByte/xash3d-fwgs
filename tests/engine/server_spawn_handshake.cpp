#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_spawn_handshake.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

static bool ReadString(NetworkBitBuffer &reader, char *out, std::size_t outSize)
{
	if (!out || outSize == 0)
		return false;

	std::size_t length = 0;
	for (;;)
	{
		const char ch = static_cast<char>(reader.readUnsigned(8));
		if (length + 1 < outSize)
			out[length++] = ch;

		if (ch == '\0')
			break;
	}

	out[outSize - 1] = '\0';
	return !reader.overflow();
}

static ServerdataPayload Payload()
{
	ServerdataPayload payload = {};
	payload.protocolVersion = 48;
	payload.spawnCount = 7;
	payload.worldMapCrc = 0x01020304u;
	payload.clientIndex = 2;
	payload.maxClients = 16;
	payload.maxEdicts = 900;
	payload.maxModels = 512;
	payload.mapName = "c1a0";
	payload.mapMessage = "Anomalous Materials";
	payload.background = true;
	payload.gameFolder = "valve";
	payload.hostFeatures = 0xAABBCCDDu;

	for (int i = 0; i < kServerdataHullComponentCount; ++i)
	{
		payload.playerMins[i] = -16 - i;
		payload.playerMaxs[i] = 16 + i;
	}

	return payload;
}

static bool TestServerdataPrintPolicyAndText()
{
	char text[128] = {};

	FormatServerdataPrintText(text, sizeof(text), 1234, 5678, 9);

	return !ShouldEmitServerdataPrint(false, 1) &&
		ShouldEmitServerdataPrint(true, 1) &&
		ShouldEmitServerdataPrint(false, 2) &&
		std::strcmp(text, "\n^3BUILD 1234 SERVER (5678 CRC)\nServer #9\n") == 0;
}

static bool TestServerdataMessageLayout()
{
	unsigned char data[256] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteServerdataMessage(writer, Payload());

	NetworkBitBuffer reader(data, writer.tellBit());
	char mapName[32] = {};
	char mapMessage[64] = {};
	char gameFolder[32] = {};

	if (reader.readUnsigned(8) != kServerdataCommand ||
		reader.readUnsigned(32) != 48 ||
		reader.readUnsigned(32) != 7 ||
		reader.readUnsigned(32) != 0x01020304u ||
		reader.readUnsigned(8) != 2 ||
		reader.readUnsigned(8) != 16 ||
		reader.readUnsigned(16) != 900 ||
		reader.readUnsigned(16) != 512 ||
		!ReadString(reader, mapName, sizeof(mapName)) ||
		!ReadString(reader, mapMessage, sizeof(mapMessage)) ||
		reader.readOneBit() != 1 ||
		!ReadString(reader, gameFolder, sizeof(gameFolder)) ||
		reader.readUnsigned(32) != 0xAABBCCDDu)
	{
		return false;
	}

	for (int i = 0; i < kServerdataHullComponentCount; ++i)
	{
		if (reader.readSigned(8) != -16 - i ||
			reader.readSigned(8) != 16 + i)
		{
			return false;
		}
	}

	return std::strcmp(mapName, "c1a0") == 0 &&
		std::strcmp(mapMessage, "Anomalous Materials") == 0 &&
		std::strcmp(gameFolder, "valve") == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterLegacyCommand()
{
	unsigned char data[256] = {};
	data[0] = kServerdataCommand;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteServerdataPayload(writer, Payload());

	return data[0] == kServerdataCommand &&
		writer.tellBit() > 8 &&
		!writer.overflow();
}

static bool TestServerdataOverflow()
{
	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteServerdataMessage(writer, Payload());
	return writer.overflow();
}

static bool TestSignonNumberMessage()
{
	static const unsigned char expected[] =
	{
		kServerdataSignonNumberCommand,
		kSpawnHandshakeDefaultSignonNumber,
	};

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSignonNumberMessage(writer, kSpawnHandshakeDefaultSignonNumber);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestCommandPlanning()
{
	return BuildNewCommandAction(false) == SpawnCommandAction::Reject &&
		BuildNewCommandAction(true) == SpawnCommandAction::SendServerdata &&
		BuildSpawnCommandAction(false, 10, 10) == SpawnCommandAction::Reject &&
		BuildSpawnCommandAction(true, 9, 10) == SpawnCommandAction::ResendNew &&
		BuildSpawnCommandAction(true, 10, 10) == SpawnCommandAction::PutClientInServer &&
		BuildBeginCommandAction(false) == SpawnCommandAction::Reject &&
		BuildBeginCommandAction(true) == SpawnCommandAction::MarkSpawned;
}

static bool TestSpawnPostPutPolicy()
{
	return BuildSpawnResendFlags() ==
			(kSpawnHandshakeResendUserinfoFlag | kSpawnHandshakeResendMovevarsFlag) &&
		ShouldSendSpawnSignon(false) &&
		!ShouldSendSpawnSignon(true) &&
		BuildSpawnSignonOverflowAction(false, 1) == SpawnOverflowAction::None &&
		BuildSpawnSignonOverflowAction(true, 1) == SpawnOverflowAction::HostError &&
		BuildSpawnSignonOverflowAction(true, 2) == SpawnOverflowAction::DropClient;
}

}

int main()
{
	if (!TestServerdataPrintPolicyAndText() ||
		!TestServerdataMessageLayout() ||
		!TestPayloadCanAppendAfterLegacyCommand() ||
		!TestServerdataOverflow() ||
		!TestSignonNumberMessage() ||
		!TestCommandPlanning() ||
		!TestSpawnPostPutPolicy())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
