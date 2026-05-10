#ifndef XASH_ENGINE_SERVER_SERVER_RESOURCE_MESSAGE_HPP
#define XASH_ENGINE_SERVER_SERVER_RESOURCE_MESSAGE_HPP

#include "engine/network/network_buffer.hpp"
#include "engine/server/resource_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kResourceMessageModelIndexBits = 12;
constexpr int kResourceMessageDownloadSizeBits = 24;
constexpr int kResourceMessageFlagsBits = 3;
constexpr int kResourceMessageResourceCountBits = 13;
constexpr std::size_t kResourceMessageHashSize = 16;
constexpr std::size_t kResourceMessageReservedSize = 32;

constexpr unsigned int kResourceMessageFlagFatalIfMissing = 1u << 0;
constexpr unsigned int kResourceMessageFlagWasMissing = 1u << 1;
constexpr unsigned int kResourceMessageFlagCustom = 1u << 2;

struct ResourceMessageRow
{
	ResourceType type;
	const char *name;
	int index;
	int downloadSize;
	unsigned int flags;
	const std::uint8_t *md5Hash;
	const std::uint8_t *reservedData;
};

bool ResourceMessageRowHasReservedData(const ResourceMessageRow &row);
void WriteResourceMessageRow(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ResourceMessageRow &row);

}
}
}

#endif
