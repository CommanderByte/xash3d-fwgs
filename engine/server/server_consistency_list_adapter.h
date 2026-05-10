#ifndef XASH_ENGINE_SERVER_CONSISTENCY_LIST_ADAPTER_H
#define XASH_ENGINE_SERVER_CONSISTENCY_LIST_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_consistency_list_write_result_s
{
	int current_bit;
	int overflow;
	int force_unmodified;
} sv_consistency_list_write_result_t;

sv_consistency_list_write_result_t SV_ConsistencyList_Write(
	const resource_t *resources,
	int resource_count,
	int consistency_count,
	int max_clients,
	int consistency_enabled,
	int hltv_proxy,
	int already_overflow,
	unsigned char *data,
	int data_bits,
	int current_bit);

#ifdef __cplusplus
}
#endif

#endif
