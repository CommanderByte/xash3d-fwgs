#include <cstdlib>

#include "engine/server/server_upload_queue.hpp"

using namespace xash::engine::server;

static ResourceDescriptor Resource(
	ResourceType type,
	int downloadSize = 0,
	unsigned int flags = 0)
{
	ResourceDescriptor resource = {};
	resource.type = type;
	resource.downloadSize = downloadSize;
	resource.flags = flags;
	return resource;
}

static bool TestDescriptorValidation()
{
	return ClientUploadResourceDescriptorIsValid(Resource(ResourceType::World, kMaxClientResourceUploadSize)) &&
		ClientUploadResourceDescriptorIsValid(Resource(ResourceType::Decal, -1)) &&
		!ClientUploadResourceDescriptorIsValid(Resource(ResourceType::Unknown, 1)) &&
		!ClientUploadResourceDescriptorIsValid(Resource(ResourceType::Generic, kMaxClientResourceUploadSize + 1));
}

static bool TestResourceListTiming()
{
	return ResourceListUpdateIsTooSoon(9.99, 10.0) &&
		!ResourceListUpdateIsTooSoon(10.0, 10.0) &&
		!ResourceListUpdateIsTooSoon(10.01, 10.0);
}

static bool TestUploadEstimate()
{
	UploadEstimateDecision ignored = DecideUploadEstimate(Resource(ResourceType::Model, 100), false);
	UploadEstimateDecision existing = DecideUploadEstimate(Resource(ResourceType::Decal, 100), true);
	UploadEstimateDecision missing = DecideUploadEstimate(Resource(ResourceType::Decal, 100), false);
	UploadEstimateDecision zero = DecideUploadEstimate(Resource(ResourceType::Decal, 0), false);

	return !ResourceShouldEstimateUploadNeed(Resource(ResourceType::Sound, 100)) &&
		ResourceShouldEstimateUploadNeed(Resource(ResourceType::Decal, 100)) &&
		ignored.action == UploadEstimateAction::Ignore &&
		existing.action == UploadEstimateAction::Ignore &&
		missing.action == UploadEstimateAction::MarkMissing &&
		missing.uploadSize == 100 &&
		zero.action == UploadEstimateAction::MissingZeroSize &&
		zero.uploadSize == 0;
}

static bool TestUploadLimit()
{
	return UploadTotalExceedsLimit(513 * 1024, 0.5) &&
		!UploadTotalExceedsLimit(512 * 1024, 0.5) &&
		!UploadTotalExceedsLimit(0, 0.0);
}

static bool TestBatchActions()
{
	const ResourceDescriptor onHand = Resource(ResourceType::Model, 100, 0);
	const ResourceDescriptor customMissing = Resource(
		ResourceType::Decal,
		100,
		kUploadResourceFlagWasMissing | kUploadResourceFlagCustom);
	const ResourceDescriptor nonCustomMissing = Resource(
		ResourceType::Decal,
		100,
		kUploadResourceFlagWasMissing);
	const ResourceDescriptor impossibleMissingModel = Resource(
		ResourceType::Model,
		100,
		kUploadResourceFlagWasMissing);

	return !UploadBatchNeedsCustomDataProbe(onHand) &&
		UploadBatchNeedsCustomDataProbe(customMissing) &&
		DecideUploadBatchAction(onHand, false, true) == UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, true, true) == UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, false, false) == UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, false, true) == UploadBatchAction::RequestCustomUpload &&
		DecideUploadBatchAction(nonCustomMissing, false, true) == UploadBatchAction::ReportNonCustomAndMove &&
		DecideUploadBatchAction(impossibleMissingModel, false, true) == UploadBatchAction::KeepInNeeded;
}

int main()
{
	if (!TestDescriptorValidation() ||
		!TestResourceListTiming() ||
		!TestUploadEstimate() ||
		!TestUploadLimit() ||
		!TestBatchActions())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
