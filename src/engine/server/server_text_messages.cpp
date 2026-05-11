#include "engine/server/server_text_messages.hpp"
#include "engine/server/server_message_envelope.hpp"

namespace xash
{
namespace engine
{
namespace server
{

void WriteTextCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command)
{
	WriteServerMessageCommand(buffer, command);
}

void WritePrintPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *text)
{
	WriteServerMessageString(buffer, text);
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
	WriteServerMessageString(buffer, commandText);
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
