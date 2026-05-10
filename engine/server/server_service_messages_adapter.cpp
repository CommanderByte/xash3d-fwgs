#include "server_service_messages_adapter.h"

#include "engine/network/network_buffer.hpp"
#include "engine/server/server_service_messages.hpp"

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

sv_service_message_write_result_t MakeResult(
	const xash::engine::network::NetworkBitBuffer &buffer)
{
	sv_service_message_write_result_t result = {};
	result.current_bit = static_cast<int>(buffer.tellBit());
	result.overflow = buffer.overflow() ? 1 : 0;
	return result;
}

}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteFileTransferFailedPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *filename)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteFileTransferFailedPayload(buffer, filename);
	return MakeResult(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteReconnectPayload(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteReconnectPayload(buffer);
	return MakeResult(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetViewPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int entity_index)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteSetViewPayload(buffer, entity_index);
	return MakeResult(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetPausePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int paused)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteSetPausePayload(buffer, paused != 0);
	return MakeResult(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteVoiceInitPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *codec,
	int quality)
{
	xash::engine::network::NetworkBitBuffer buffer =
		MakeBuffer(data, data_bits, current_bit);
	xash::engine::server::WriteVoiceInitPayload(buffer, codec, quality);
	return MakeResult(buffer);
}
