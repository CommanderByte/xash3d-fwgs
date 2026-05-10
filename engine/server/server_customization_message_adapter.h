#ifndef XASH_ENGINE_SERVER_CUSTOMIZATION_MESSAGE_ADAPTER_H
#define XASH_ENGINE_SERVER_CUSTOMIZATION_MESSAGE_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_customization_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_customization_message_write_result_t;

sv_customization_message_write_result_t SV_CustomizationMessage_WritePayload(
	const resource_t *resource,
	int playernum,
	unsigned char *data,
	int data_bits,
	int current_bit);

#ifdef __cplusplus
}
#endif

#endif
