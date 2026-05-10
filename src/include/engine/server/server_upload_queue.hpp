#ifndef XASH_ENGINE_SERVER_SERVER_UPLOAD_QUEUE_HPP
#define XASH_ENGINE_SERVER_SERVER_UPLOAD_QUEUE_HPP

#include "engine/server/resource_identity.hpp"

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kMaxClientResourceUploadSize = 1024 * 1024 * 1024;
constexpr unsigned int kUploadResourceFlagWasMissing = 1u << 1;
constexpr unsigned int kUploadResourceFlagCustom = 1u << 2;

enum class UploadEstimateAction
{
	Ignore,
	MarkMissing,
	MissingZeroSize,
};

struct UploadEstimateDecision
{
	UploadEstimateAction action;
	int uploadSize;
};

enum class UploadBatchAction
{
	MoveToOnHand,
	RequestCustomUpload,
	ReportNonCustomAndMove,
	KeepInNeeded,
};

bool ClientUploadResourceDescriptorIsValid(const ResourceDescriptor &resource);
bool ResourceListUpdateIsTooSoon(double now, double nextAllowedTime);
bool ResourceShouldEstimateUploadNeed(const ResourceDescriptor &resource);
UploadEstimateDecision DecideUploadEstimate(
	const ResourceDescriptor &resource,
	bool hpakContainsResource);
bool UploadTotalExceedsLimit(int totalBytes, double maxUploadMiB);
bool UploadBatchNeedsCustomDataProbe(const ResourceDescriptor &resource);
UploadBatchAction DecideUploadBatchAction(
	const ResourceDescriptor &resource,
	bool customResourceDataExists,
	bool allowUpload);

}
}
}

#endif
