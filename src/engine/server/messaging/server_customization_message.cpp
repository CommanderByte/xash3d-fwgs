#include "engine/server/messaging/server_customization_message.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

void WriteByte(xash::engine::network::NetworkBitBuffer &buffer, unsigned int value)
{
	buffer.writeUnsigned(static_cast<std::uint8_t>(value), 8);
}

void WriteString(xash::engine::network::NetworkBitBuffer &buffer, const char *value)
{
	if (!value)
		value = "";

	const std::size_t length = std::strlen(value);
	for (std::size_t i = 0; i <= length; ++i)
		WriteByte(buffer, static_cast<unsigned char>(value[i]));
}

void WriteFixedBytes(
	xash::engine::network::NetworkBitBuffer &buffer,
	const std::uint8_t *data,
	std::size_t size)
{
	static const std::uint8_t zeros[kCustomizationMessageHashSize] = {};
	const std::uint8_t *bytes = data ? data : zeros;

	for (std::size_t i = 0; i < size; ++i)
		WriteByte(buffer, bytes[i]);
}

}

void WriteCustomizationMessagePayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const CustomizationMessage &message)
{
	WriteByte(buffer, static_cast<unsigned int>(message.playerNumber));
	WriteByte(buffer, static_cast<unsigned int>(message.type));
	WriteString(buffer, message.name);
	buffer.writeSigned(message.index, 16);
	buffer.writeSigned(message.downloadSize, 32);
	WriteByte(buffer, message.flags);

	if ((message.flags & kCustomizationMessageFlagCustom) != 0)
		WriteFixedBytes(buffer, message.md5Hash, kCustomizationMessageHashSize);
}

}
}
}
