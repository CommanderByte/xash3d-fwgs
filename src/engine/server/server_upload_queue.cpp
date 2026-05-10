#include "engine/server/server_upload_queue.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool HasFlag(unsigned int flags, unsigned int flag)
{
	return (flags & flag) != 0;
}

bool IsDecal(const ResourceDescriptor &resource)
{
	return resource.type == ResourceType::Decal;
}

}

bool ClientUploadResourceDescriptorIsValid(const ResourceDescriptor &resource)
{
	return resource.type != ResourceType::Unknown &&
		resource.downloadSize <= kMaxClientResourceUploadSize;
}

bool ResourceListUpdateIsTooSoon(double now, double nextAllowedTime)
{
	return now < nextAllowedTime;
}

bool ResourceShouldEstimateUploadNeed(const ResourceDescriptor &resource)
{
	return IsDecal(resource);
}

UploadEstimateDecision DecideUploadEstimate(
	const ResourceDescriptor &resource,
	bool hpakContainsResource)
{
	UploadEstimateDecision decision = {};
	decision.action = UploadEstimateAction::Ignore;

	if (!ResourceShouldEstimateUploadNeed(resource) || hpakContainsResource)
		return decision;

	if (resource.downloadSize != 0)
	{
		decision.action = UploadEstimateAction::MarkMissing;
		decision.uploadSize = resource.downloadSize;
		return decision;
	}

	decision.action = UploadEstimateAction::MissingZeroSize;
	return decision;
}

bool UploadTotalExceedsLimit(int totalBytes, double maxUploadMiB)
{
	return static_cast<double>(totalBytes) > maxUploadMiB * 1024.0 * 1024.0;
}

bool UploadBatchNeedsCustomDataProbe(const ResourceDescriptor &resource)
{
	return HasFlag(resource.flags, kUploadResourceFlagWasMissing) &&
		IsDecal(resource) &&
		HasFlag(resource.flags, kUploadResourceFlagCustom);
}

UploadBatchAction DecideUploadBatchAction(
	const ResourceDescriptor &resource,
	bool customResourceDataExists,
	bool allowUpload)
{
	if (!HasFlag(resource.flags, kUploadResourceFlagWasMissing))
		return UploadBatchAction::MoveToOnHand;

	if (!IsDecal(resource))
		return UploadBatchAction::KeepInNeeded;

	if (!HasFlag(resource.flags, kUploadResourceFlagCustom))
		return UploadBatchAction::ReportNonCustomAndMove;

	if (customResourceDataExists || !allowUpload)
		return UploadBatchAction::MoveToOnHand;

	return UploadBatchAction::RequestCustomUpload;
}

}
}
}
