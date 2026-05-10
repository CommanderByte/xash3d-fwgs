#ifndef XASH_ENGINE_SERVER_RESOURCE_MESSAGE_ADAPTER_H
#define XASH_ENGINE_SERVER_RESOURCE_MESSAGE_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_resource_message_write_result_s
{
	int current_bit;
	int overflow;
} sv_resource_message_write_result_t;

sv_resource_message_write_result_t SV_ResourceMessage_WriteResource(
	const resource_t *resource,
	unsigned char *data,
	int data_bits,
	int current_bit);

#ifdef __cplusplus
}
#endif

#endif
