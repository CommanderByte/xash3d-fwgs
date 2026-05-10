#ifndef XASH_ENGINE_SERVER_UPLOAD_QUEUE_ADAPTER_H
#define XASH_ENGINE_SERVER_UPLOAD_QUEUE_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_upload_estimate_action_e
{
	SV_UPLOAD_ESTIMATE_IGNORE = 0,
	SV_UPLOAD_ESTIMATE_MARK_MISSING,
	SV_UPLOAD_ESTIMATE_MISSING_ZERO_SIZE,
};

typedef struct sv_upload_estimate_decision_s
{
	enum sv_upload_estimate_action_e action;
	int upload_size;
} sv_upload_estimate_decision_t;

enum sv_upload_batch_action_e
{
	SV_UPLOAD_BATCH_MOVE_TO_ON_HAND = 0,
	SV_UPLOAD_BATCH_REQUEST_CUSTOM_UPLOAD,
	SV_UPLOAD_BATCH_REPORT_NON_CUSTOM_AND_MOVE,
	SV_UPLOAD_BATCH_KEEP_IN_NEEDED,
};

int SV_UploadQueue_IsClientResourceDescriptorValid(const resource_t *resource);
int SV_UploadQueue_ResourceListUpdateIsTooSoon(double now, double next_allowed_time);
int SV_UploadQueue_ShouldEstimateUploadNeed(const resource_t *resource);
sv_upload_estimate_decision_t SV_UploadQueue_DecideUploadEstimate(
	const resource_t *resource,
	int hpak_contains_resource);
int SV_UploadQueue_UploadTotalExceedsLimit(int total_bytes, float max_upload_mib);
int SV_UploadQueue_BatchNeedsCustomDataProbe(const resource_t *resource);
enum sv_upload_batch_action_e SV_UploadQueue_DecideBatchAction(
	const resource_t *resource,
	int custom_resource_data_exists,
	int allow_upload);

#ifdef __cplusplus
}
#endif

#endif
