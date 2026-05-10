#ifndef XASH_ENGINE_SERVER_CONSISTENCY_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_CONSISTENCY_POLICY_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_consistency_entry_status_e
{
	SV_CONSISTENCY_ENTRY_MATCH = 0,
	SV_CONSISTENCY_ENTRY_BAD_RESOURCE = 1,
	SV_CONSISTENCY_ENTRY_INVALID_TYPE = 2,
};

typedef struct sv_consistency_reservation_result_s
{
	int requires_model_bounds;
	int has_reserved_data;
	int unsupported_force_type;
	unsigned char reserved_data[32];
} sv_consistency_reservation_result_t;

typedef struct sv_consistency_entry_decision_s
{
	int status;
	int resource_index;
} sv_consistency_entry_decision_t;

int SV_ConsistencyPolicy_ResourceNeedsSetup(int already_check_file, int in_consistency_list);
int SV_ConsistencyPolicy_RequiresModelBounds(resourcetype_t resource_type, int force_type);
sv_consistency_reservation_result_t SV_ConsistencyPolicy_BuildReservation(
	resourcetype_t resource_type,
	int force_type,
	const float *specified_mins,
	const float *specified_maxs,
	const float *model_mins,
	const float *model_maxs,
	int model_bounds_available);
int SV_ConsistencyPolicy_ReservedDataIsEmpty(const unsigned char *reserved_data);
sv_consistency_entry_decision_t SV_ConsistencyPolicy_EvaluateChecksum(
	int resource_index,
	const unsigned char *expected_hash,
	const unsigned char *received_prefix,
	int received_prefix_size);
sv_consistency_entry_decision_t SV_ConsistencyPolicy_EvaluateBounds(
	int resource_index,
	const unsigned char *reserved_data,
	const float *client_mins,
	const float *client_maxs);
int SV_ConsistencyPolicy_ResponseCountMatches(int expected_count, int actual_count);

#ifdef __cplusplus
}
#endif

#endif
