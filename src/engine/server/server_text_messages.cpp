#include "engine/server/server_text_messages.hpp"

#include <cstddef>
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

void WriteTextCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteByte(buffer, command);
}

void WritePrintPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *text)
{
	WriteString(buffer, text);
}

void WritePrintMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *text)
{
	WriteTextCommand(buffer, kTextMessagePrint);
	WritePrintPayload(buffer, text);
}

void WriteStuffTextPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *commandText)
{
	WriteString(buffer, commandText);
}

void WriteStuffTextMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *commandText)
{
	WriteTextCommand(buffer, kTextMessageStuffText);
	WriteStuffTextPayload(buffer, commandText);
}


}
}
}
