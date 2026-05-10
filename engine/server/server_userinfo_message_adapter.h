#ifndef XASH_ENGINE_SERVER_SERVER_USERINFO_MESSAGE_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_USERINFO_MESSAGE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_userinfo_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_userinfo_message_write_result_t;

sv_userinfo_message_write_result_t SV_UserinfoMessage_WritePayload(
	unsigned char *data,
	int data_bits,
	int current_bit,
	int client_index,
	int user_id,
	int active,
	const char *userinfo,
	const unsigned char digest[16]);

#ifdef __cplusplus
}
#endif

#endif
