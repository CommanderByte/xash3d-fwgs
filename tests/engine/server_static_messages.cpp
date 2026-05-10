#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_static_messages.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

static StaticDecalPayload BasicDecal()
{
	StaticDecalPayload payload = {};
	payload.origin[0] = 1.25f;
	payload.origin[1] = -2.5f;
	payload.origin[2] = 0.0f;
	payload.decalIndex = 300;
	payload.entityIndex = 12;
	payload.modelIndex = 34;
	payload.flags = 0xA5;
	payload.scale = 1.5f;
	payload.largeCoordinates = false;
	return payload;
}

static bool ReadDecalPayload(
	NetworkBitBuffer &reader,
	StaticDecalPayload *payload,
	int *encodedX,
	int *encodedY,
	int *encodedZ,
	int *encodedScale)
{
	*encodedX = reader.readSigned(16);
	*encodedY = reader.readSigned(16);
	*encodedZ = reader.readSigned(16);
	payload->decalIndex = static_cast<int>(reader.readUnsigned(16));
	payload->entityIndex = reader.readSigned(16);
	payload->modelIndex = payload->entityIndex > 0 ?
		static_cast<int>(reader.readUnsigned(16)) :
		0;
	payload->flags = static_cast<int>(reader.readUnsigned(8));
	*encodedScale = static_cast<int>(reader.readUnsigned(16));
	payload->scale = static_cast<float>(*encodedScale) / 4096.0f;
	return !reader.overflow();
}

static bool TestBspDecalMessageWithEntity()
{
	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteBspDecalMessage(writer, BasicDecal());

	NetworkBitBuffer reader(data, writer.tellBit());
	StaticDecalPayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;
	int scale = 0;

	return reader.readUnsigned(8) == kStaticMessageBspDecalCommand &&
		ReadDecalPayload(reader, &actual, &x, &y, &z, &scale) &&
		x == 10 &&
		y == -20 &&
		z == 0 &&
		actual.decalIndex == 300 &&
		actual.entityIndex == 12 &&
		actual.modelIndex == 34 &&
		actual.flags == 0xA5 &&
		scale == 6144 &&
		reader.tellBit() == writer.tellBit() &&
		!writer.overflow();
}

static bool TestBspDecalOmitsModelIndexForWorldEntity()
{
	StaticDecalPayload payload = BasicDecal();
	payload.entityIndex = 0;
	payload.modelIndex = 777;

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteBspDecalMessage(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	StaticDecalPayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;
	int scale = 0;

	return reader.readUnsigned(8) == kStaticMessageBspDecalCommand &&
		ReadDecalPayload(reader, &actual, &x, &y, &z, &scale) &&
		actual.entityIndex == 0 &&
		actual.modelIndex == 0 &&
		writer.tellBit() == ((1 + 6 + 2 + 2 + 1 + 2) * 8) &&
		!writer.overflow();
}

static bool TestLargeCoordinatesRoundLikeLegacy()
{
	StaticDecalPayload payload = BasicDecal();
	payload.largeCoordinates = true;
	payload.origin[0] = 1.1f;
	payload.origin[1] = -1.1f;
	payload.origin[2] = 123.6f;

	unsigned char data[64] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteBspDecalMessage(writer, payload);

	NetworkBitBuffer reader(data, writer.tellBit());
	StaticDecalPayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;
	int scale = 0;

	return reader.readUnsigned(8) == kStaticMessageBspDecalCommand &&
		ReadDecalPayload(reader, &actual, &x, &y, &z, &scale) &&
		x == 1 &&
		y == -1 &&
		z == 124 &&
		!writer.overflow();
}

static bool TestPayloadCanAppendAfterLegacyCommand()
{
	unsigned char data[64] = {};
	data[0] = kStaticMessageBspDecalCommand;

	NetworkBitBuffer writer(data, sizeof(data) << 3, 8);
	WriteBspDecalPayload(writer, BasicDecal());

	NetworkBitBuffer reader(data, writer.tellBit());
	StaticDecalPayload actual = {};
	int x = 0;
	int y = 0;
	int z = 0;
	int scale = 0;

	return data[0] == kStaticMessageBspDecalCommand &&
		reader.readUnsigned(8) == kStaticMessageBspDecalCommand &&
		ReadDecalPayload(reader, &actual, &x, &y, &z, &scale) &&
		actual.decalIndex == 300 &&
		!writer.overflow();
}

static bool TestSpawnStaticCommandByte()
{
	static const unsigned char expected[] =
	{
		kStaticMessageSpawnStaticCommand,
	};

	unsigned char data[8] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteSpawnStaticCommand(writer);

	return writer.tellBit() == sizeof(expected) * 8 &&
		std::memcmp(data, expected, sizeof(expected)) == 0 &&
		!writer.overflow();
}

static bool TestSpawnStaticDecisionAllowsNormalCase()
{
	const SpawnStaticDecision decision =
		BuildSpawnStaticDecision(10, 128, kSpawnStaticMinimumBytesLeft);

	return decision.shouldWrite &&
		decision.reason == SpawnStaticRejectReason::None;
}

static bool TestSpawnStaticDecisionRejectsLastReservedSlot()
{
	const SpawnStaticDecision decision =
		BuildSpawnStaticDecision(127, 128, kSpawnStaticMinimumBytesLeft);

	return !decision.shouldWrite &&
		decision.reason == SpawnStaticRejectReason::TooManyStaticEntities;
}

static bool TestSpawnStaticDecisionRejectsSmallBuffer()
{
	const SpawnStaticDecision decision =
		BuildSpawnStaticDecision(10, 128, kSpawnStaticMinimumBytesLeft - 1);

	return !decision.shouldWrite &&
		decision.reason == SpawnStaticRejectReason::BufferTooSmall;
}

static bool TestOverflow()
{
	unsigned char data[4] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteBspDecalMessage(writer, BasicDecal());
	return writer.overflow();
}

int main()
{
	if (!TestBspDecalMessageWithEntity() ||
		!TestBspDecalOmitsModelIndexForWorldEntity() ||
		!TestLargeCoordinatesRoundLikeLegacy() ||
		!TestPayloadCanAppendAfterLegacyCommand() ||
		!TestSpawnStaticCommandByte() ||
		!TestSpawnStaticDecisionAllowsNormalCase() ||
		!TestSpawnStaticDecisionRejectsLastReservedSlot() ||
		!TestSpawnStaticDecisionRejectsSmallBuffer() ||
		!TestOverflow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
