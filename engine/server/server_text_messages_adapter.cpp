#include "server_text_messages_adapter.h"

#include "engine/server/messaging/server_text_messages.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_text_message_write_result_t SV_TextMessage_WritePrintPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *text)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_text_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WritePrintPayload,
			text);
}

extern "C" sv_text_message_write_result_t SV_TextMessage_WriteStuffTextPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *command_text)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_text_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteStuffTextPayload,
			command_text);
}
