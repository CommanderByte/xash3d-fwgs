#ifndef XASH_ENGINE_SERVER_SERVER_TEXT_MESSAGES_HPP
#define XASH_ENGINE_SERVER_SERVER_TEXT_MESSAGES_HPP

#include "engine/network/network_buffer.hpp"

#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint8_t kTextMessagePrint = 8;
constexpr std::uint8_t kTextMessageStuffText = 9;

void WriteTextCommand(
	xash::engine::network::NetworkBitBuffer &buffer,
	std::uint8_t command);

void WritePrintPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *text);
void WritePrintMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *text);

void WriteStuffTextPayload(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *commandText);
void WriteStuffTextMessage(
	xash::engine::network::NetworkBitBuffer &buffer,
	const char *commandText);

}
}
}

#endif
