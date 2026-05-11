#include "server_text_messages_adapter.h"

#include "engine/server/server_text_messages.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_text_message_write_result_t SV_TextMessage_WritePrintPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *text)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WritePrintPayload(buffer, text);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_text_message_write_result_t>(buffer);
}

extern "C" sv_text_message_write_result_t SV_TextMessage_WriteStuffTextPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *command_text)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteStuffTextPayload(buffer, command_text);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_text_message_write_result_t>(buffer);
}
