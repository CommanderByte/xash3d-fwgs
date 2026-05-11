#include "engine/server/server_static_messages.hpp"
#include "engine/server/server_message_envelope.hpp"

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
	int value)
{
	buffer.writeUnsigned(static_cast<std::uint32_t>(value), 16);
}

void WriteShort(
	xash::engine::network::NetworkBitBuffer &buffer,
	int value)
{
	buffer.writeSigned(value, 16);
}

int RoundLikeLegacy(float value)
{
	return value < 0.0f ?
		static_cast<int>(value - 0.5f) :
		static_cast<int>(value + 0.5f);
}

int EncodeCoordinate(float value, bool largeCoordinates)
{
	if (largeCoordinates)
		return RoundLikeLegacy(value);

	return static_cast<int>(value * 8.0f);
}

void WriteCoordinate(
	xash::engine::network::NetworkBitBuffer &buffer,
	float value,
	bool largeCoordinates)
{
	WriteShort(buffer, EncodeCoordinate(value, largeCoordinates));
}

int EncodeScale(float scale)
{
	return static_cast<int>(scale * 4096.0f);
}

}

SpawnStaticDecision BuildSpawnStaticDecision(
	int index,
	int maxStaticEntities,
	int bytesLeft)
{
	SpawnStaticDecision decision = {};
	decision.shouldWrite = false;
	decision.reason = SpawnStaticRejectReason::None;

	if (index >= maxStaticEntities - 1)
	{
		decision.reason = SpawnStaticRejectReason::TooManyStaticEntities;
		return decision;
	}

	if (bytesLeft < kSpawnStaticMinimumBytesLeft)
	{
		decision.reason = SpawnStaticRejectReason::BufferTooSmall;
		return decision;
	}

	decision.shouldWrite = true;
	return decision;
}

void WriteStaticMessageCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteServerMessageCommand(buffer, command);
}

void WriteBspDecalPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const StaticDecalPayload &payload)
{
	WriteCoordinate(buffer, payload.origin[0], payload.largeCoordinates);
	WriteCoordinate(buffer, payload.origin[1], payload.largeCoordinates);
	WriteCoordinate(buffer, payload.origin[2], payload.largeCoordinates);
	WriteWord(buffer, payload.decalIndex);
	WriteShort(buffer, payload.entityIndex);
	if (payload.entityIndex > 0)
		WriteWord(buffer, payload.modelIndex);
	WriteServerMessageByte(buffer, static_cast<unsigned int>(payload.flags));
	WriteWord(buffer, EncodeScale(payload.scale));
}

void WriteBspDecalMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const StaticDecalPayload &payload)
{
	WriteStaticMessageCommand(buffer, kStaticMessageBspDecalCommand);
	WriteBspDecalPayload(buffer, payload);
}

void WriteSpawnStaticCommand(
	xash::engine::network::NetworkBitBuffer &buffer)
{
	WriteStaticMessageCommand(buffer, kStaticMessageSpawnStaticCommand);
}


}
}
}
