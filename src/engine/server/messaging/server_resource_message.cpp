#include "engine/server/messaging/server_resource_message.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

void WriteString(xash::engine::network::NetworkBitBuffer &buffer, const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		buffer.writeUnsigned(static_cast<std::uint8_t>(value[i]), 8);
}

void WriteFixedBytes(
	xash::engine::network::NetworkBitBuffer &buffer,
	const std::uint8_t *data,
	std::size_t size)
{
	static const std::uint8_t zeros[kResourceMessageReservedSize] = {};
	const std::uint8_t *bytes = data ? data : zeros;

	for (std::size_t i = 0; i < size; ++i)
		buffer.writeUnsigned(bytes[i], 8);
}

}

bool ResourceMessageRowHasReservedData(const ResourceMessageRow &row)
{
	if (!row.reservedData)
		return false;

	static const std::uint8_t zeros[kResourceMessageReservedSize] = {};
	return std::memcmp(row.reservedData, zeros, kResourceMessageReservedSize) != 0;
}

void WriteResourceMessageRow(
	xash::engine::network::NetworkBitBuffer &buffer,
	const ResourceMessageRow &row)
{
	const unsigned int transmittedFlags =
		row.flags & (kResourceMessageFlagFatalIfMissing | kResourceMessageFlagWasMissing);

	buffer.writeUnsigned(static_cast<std::uint32_t>(row.type), 4);
	WriteString(buffer, row.name);
	buffer.writeUnsigned(static_cast<std::uint32_t>(row.index), kResourceMessageModelIndexBits);
	buffer.writeSigned(row.downloadSize, kResourceMessageDownloadSizeBits);
	buffer.writeUnsigned(transmittedFlags, kResourceMessageFlagsBits);

	if ((row.flags & kResourceMessageFlagCustom) != 0)
		WriteFixedBytes(buffer, row.md5Hash, kResourceMessageHashSize);

	const bool hasReservedData = ResourceMessageRowHasReservedData(row);
	buffer.writeOneBit(hasReservedData ? 1 : 0);

	if (hasReservedData)
		WriteFixedBytes(buffer, row.reservedData, kResourceMessageReservedSize);
}

}
}
}
