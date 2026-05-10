#include "server_text_messages_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_text_messages.hpp"

#include <cstddef>

namespace
{

xash::engine::network::NetworkBitBuffer MakeBuffer(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	return xash::engine::network::NetworkBitBuffer(
		data,
		data_bits < 0 ? 0U : static_cast<std::size_t>(data_bits),
		current_bit < 0 ? 0U : static_cast<std::size_t>(current_bit));
}

sv_text_message_write_result_t MakeResult(
	const xash::engine::network::NetworkBitBuffer &buffer)
{
	sv_text_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}

}

extern "C" sv_text_message_write_result_t SV_TextMessage_WritePrintPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *text)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WritePrintPayload(buffer, text);
	return MakeResult(buffer);
}

extern "C" sv_text_message_write_result_t SV_TextMessage_WriteStuffTextPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *command_text)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteStuffTextPayload(buffer, command_text);
	return MakeResult(buffer);
}
