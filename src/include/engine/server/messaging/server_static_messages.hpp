#ifndef XASH_ENGINE_SERVER_SERVER_STATIC_MESSAGES_HPP
#define XASH_ENGINE_SERVER_SERVER_STATIC_MESSAGES_HPP

#include "engine/network/network_buffer.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kStaticMessageSpawnStaticCommand = 20;
constexpr std::uint8_t kStaticMessageBspDecalCommand = 36;
constexpr int kSpawnStaticMinimumBytesLeft = 50;

enum class SpawnStaticRejectReason
{
	None,
	TooManyStaticEntities,
	BufferTooSmall
};

struct SpawnStaticDecision
{
	bool shouldWrite;
	SpawnStaticRejectReason reason;
};

struct StaticDecalPayload
{
	float origin[3];
	int decalIndex;
	int entityIndex;
	int modelIndex;
	int flags;
	float scale;
	bool largeCoordinates;
};

SpawnStaticDecision BuildSpawnStaticDecision(
	int index,
	int maxStaticEntities,
	int bytesLeft);

void WriteStaticMessageCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command);

void WriteBspDecalPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const StaticDecalPayload &payload);
void WriteBspDecalMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const StaticDecalPayload &payload);

void WriteSpawnStaticCommand(
	xash::engine::network::NetworkBitBuffer &buffer);

}
}
}

#endif
