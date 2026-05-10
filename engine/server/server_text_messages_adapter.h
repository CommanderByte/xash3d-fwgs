#ifndef XASH_ENGINE_SERVER_SERVER_TEXT_MESSAGES_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_TEXT_MESSAGES_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_text_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_text_message_write_result_t;

sv_text_message_write_result_t SV_TextMessage_WritePrintPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *text);

sv_text_message_write_result_t SV_TextMessage_WriteStuffTextPayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	const char *command_text);

#ifdef __cplusplus
}
#endif

#endif
