#ifndef XASH_ENGINE_SERVER_SERVER_SERVICE_MESSAGES_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_SERVICE_MESSAGES_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_service_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_service_message_write_result_t;

sv_service_message_write_result_t SV_ServiceMessage_WriteFileTransferFailedPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *filename);

sv_service_message_write_result_t SV_ServiceMessage_WriteReconnectPayload(
	unsigned char *data,
	int data_bits,
	int current_bit);

sv_service_message_write_result_t SV_ServiceMessage_WriteSetViewPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int entity_index);

sv_service_message_write_result_t SV_ServiceMessage_WriteSetPausePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int paused);

sv_service_message_write_result_t SV_ServiceMessage_WriteVoiceInitPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *codec,
	int quality);

#ifdef __cplusplus
}
#endif

#endif
