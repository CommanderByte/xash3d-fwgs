#include "server_upload_queue_adapter.h"

#include "engine/server/server_upload_queue.hpp"
#include "resource_adapter_shared.hpp"

namespace adapter = xash::engine::server::adapter;

namespace
{

sv_upload_estimate_decision_t ToLegacyDecision(
	const xash::engine::server::UploadEstimateDecision &decision)
{
	using xash::engine::server::UploadEstimateAction;

	sv_upload_estimate_decision_t legacy = {};
	legacy.upload_size = decision.uploadSize;

	switch (decision.action)
	{
	case UploadEstimateAction::Ignore:
		legacy.action = SV_UPLOAD_ESTIMATE_IGNORE;
		break;
	case UploadEstimateAction::MarkMissing:
		legacy.action = SV_UPLOAD_ESTIMATE_MARK_MISSING;
		break;
	case UploadEstimateAction::MissingZeroSize:
		legacy.action = SV_UPLOAD_ESTIMATE_MISSING_ZERO_SIZE;
		break;
	}

	return legacy;
}

enum sv_upload_batch_action_e ToLegacyAction(xash::engine::server::UploadBatchAction action)
{
	using xash::engine::server::UploadBatchAction;

	switch (action)
	{
	case UploadBatchAction::MoveToOnHand:
		return SV_UPLOAD_BATCH_MOVE_TO_ON_HAND;
	case UploadBatchAction::RequestCustomUpload:
		return SV_UPLOAD_BATCH_REQUEST_CUSTOM_UPLOAD;
	case UploadBatchAction::ReportNonCustomAndMove:
		return SV_UPLOAD_BATCH_REPORT_NON_CUSTOM_AND_MOVE;
	case UploadBatchAction::KeepInNeeded:
	default:
		return SV_UPLOAD_BATCH_KEEP_IN_NEEDED;
	}
}

}

extern "C" int SV_UploadQueue_IsClientResourceDescriptorValid(const resource_t *resource)
{
	return xash::engine::server::ClientUploadResourceDescriptorIsValid(
		adapter::ToModernResourceDescriptor(resource)) ? 1 : 0;
}

extern "C" int SV_UploadQueue_ResourceListUpdateIsTooSoon(
	double now,
	double next_allowed_time)
{
	return xash::engine::server::ResourceListUpdateIsTooSoon(
		now,
		next_allowed_time) ? 1 : 0;
}

extern "C" int SV_UploadQueue_ShouldEstimateUploadNeed(const resource_t *resource)
{
	return xash::engine::server::ResourceShouldEstimateUploadNeed(
		adapter::ToModernResourceDescriptor(resource)) ? 1 : 0;
}

extern "C" sv_upload_estimate_decision_t SV_UploadQueue_DecideUploadEstimate(
	const resource_t *resource,
	int hpak_contains_resource)
{
	return ToLegacyDecision(xash::engine::server::DecideUploadEstimate(
		adapter::ToModernResourceDescriptor(resource),
		hpak_contains_resource != 0));
}

extern "C" int SV_UploadQueue_UploadTotalExceedsLimit(
	int total_bytes,
	float max_upload_mib)
{
	return xash::engine::server::UploadTotalExceedsLimit(
		total_bytes,
		max_upload_mib) ? 1 : 0;
}

extern "C" int SV_UploadQueue_BatchNeedsCustomDataProbe(const resource_t *resource)
{
	return xash::engine::server::UploadBatchNeedsCustomDataProbe(
		adapter::ToModernResourceDescriptor(resource)) ? 1 : 0;
}

extern "C" enum sv_upload_batch_action_e SV_UploadQueue_DecideBatchAction(
	const resource_t *resource,
	int custom_resource_data_exists,
	int allow_upload)
{
	return ToLegacyAction(xash::engine::server::DecideUploadBatchAction(
		adapter::ToModernResourceDescriptor(resource),
		custom_resource_data_exists != 0,
		allow_upload != 0));
}
