#include "server_service_messages_adapter.h"

#include "engine/server/messaging/server_service_messages.hpp"
#include "server_message_adapter_shared.hpp"

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteFileTransferFailedPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *filename)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_service_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteFileTransferFailedPayload,
			filename);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteReconnectPayload(
	unsigned char *data,
	int data_bits,
	int current_bit)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_service_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteReconnectPayload);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetViewPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int entity_index)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_service_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteSetViewPayload,
			entity_index);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteSetPausePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int paused)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_service_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteSetPausePayload,
			paused != 0);
}

extern "C" sv_service_message_write_result_t SV_ServiceMessage_WriteVoiceInitPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *codec,
	int quality)
{
	return xash::engine::server::adapter::WritePayloadWithNetworkBitBuffer<
		sv_service_message_write_result_t>(
			data,
			data_bits,
			current_bit,
			xash::engine::server::WriteVoiceInitPayload,
			codec,
			quality);
}
