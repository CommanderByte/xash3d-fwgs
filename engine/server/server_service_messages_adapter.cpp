#include "server_service_messages_adapter.h"

#include "engine/server/server_service_messages.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteFileTransferFailedPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *filename)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteFileTransferFailedPayload(buffer, filename);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_service_message_write_result_t>(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteReconnectPayload(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteReconnectPayload(buffer);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_service_message_write_result_t>(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetViewPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int entity_index)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteSetViewPayload(buffer, entity_index);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_service_message_write_result_t>(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetPausePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int paused)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteSetPausePayload(buffer, paused != 0);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_service_message_write_result_t>(buffer);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteVoiceInitPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *codec,
	int quality)
{
	xash::engine::network::NetworkBitBuffer buffer =
		xash::engine::server::adapter::MakeNetworkBitBuffer(
			data,
			data_bits,
			current_bit);
	xash::engine::server::WriteVoiceInitPayload(buffer, codec, quality);
	return xash::engine::server::adapter::MakeWriteResult<
		sv_service_message_write_result_t>(buffer);
}
