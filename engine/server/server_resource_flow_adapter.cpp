#include "server_download_policy_adapter.h"
#include "server_hot_resource_adapter.h"
#include "server_reslist_policy_adapter.h"
#include "server_resource_catalog_adapter.h"
#include "server_upload_queue_adapter.h"

#include "engine/server/resources/server_download_policy.hpp"
#include "engine/server/resources/server_hot_resource.hpp"
#include "engine/server/resources/server_reslist_policy.hpp"
#include "engine/server/resources/server_resource_catalog.hpp"
#include "engine/server/resources/server_upload_queue.hpp"
#include "resource_adapter_shared.hpp"
#include "utilities/path.hpp"

#include <cstring>
#include <vector>

extern "C" int Sound_SupportedFileFormat(const char *fileext);

namespace adapter = xash::engine::server::adapter;

namespace
{

sv_resource_catalog_entry_t ToLegacyCatalogEntry(
	const xash::engine::server::ResourceCatalogEntry &entry)
{
	const adapter::LegacyResourceDescriptorFields fields =
		adapter::ToLegacyResourceDescriptorFields(entry.resource);
	sv_resource_catalog_entry_t legacy = {};
	legacy.should_add = entry.shouldAdd ? 1 : 0;
	legacy.type = fields.type;
	legacy.name = fields.name;
	legacy.download_size = fields.downloadSize;
	legacy.flags = fields.flags;
	legacy.index = fields.index;
	return legacy;
}

xash::engine::server::ResourceCatalogState ToModernCatalogState(
	const sv_resource_catalog_state_t *state)
{
	xash::engine::server::ResourceCatalogState modern = {};
	modern.soundSentenceMarkerAdded =
		state && state->sound_sentence_marker_added != 0;
	return modern;
}

void CopyModernCatalogState(
	sv_resource_catalog_state_t *legacy,
	const xash::engine::server::ResourceCatalogState &modern)
{
	if (!legacy)
		return;

	legacy->sound_sentence_marker_added =
		modern.soundSentenceMarkerAdded ? 1 : 0;
}

sv_download_policy_decision_t ToLegacyDownloadDecision(
	const xash::engine::server::ServerDownloadDecision &decision)
{
	using xash::engine::server::ServerDownloadAction;

	sv_download_policy_decision_t legacy = {};
	legacy.resource_index = decision.resourceIndex;
	legacy.file_name = decision.fileName;
	legacy.model_texture_name = decision.modelTextureName;
	std::memcpy(
		legacy.custom_hash,
		decision.customHash,
		sizeof(legacy.custom_hash));

	switch (decision.action)
	{
	case ServerDownloadAction::Ignore:
		legacy.action = SV_DOWNLOAD_POLICY_IGNORE;
		break;
	case ServerDownloadAction::Reject:
		legacy.action = SV_DOWNLOAD_POLICY_REJECT;
		break;
	case ServerDownloadAction::SendFile:
		legacy.action = SV_DOWNLOAD_POLICY_SEND_FILE;
		break;
	case ServerDownloadAction::SendFileWithModelTexture:
		legacy.action = SV_DOWNLOAD_POLICY_SEND_FILE_WITH_MODEL_TEXTURE;
		break;
	case ServerDownloadAction::LookupCustomLogo:
		legacy.action = SV_DOWNLOAD_POLICY_LOOKUP_CUSTOM_LOGO;
		break;
	}

	return legacy;
}

sv_upload_estimate_decision_t ToLegacyUploadEstimateDecision(
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

enum sv_upload_batch_action_e ToLegacyUploadBatchAction(
	xash::engine::server::UploadBatchAction action)
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

xash::engine::server::HotResourceRequest ToModernHotResourceRequest(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags)
{
	xash::engine::server::HotResourceRequest request = {};
	request.type = adapter::ToModernResourceType(type);
	request.name = name;
	request.index = index;
	request.flags = flags;
	return request;
}

int ToLegacyReslistRoute(xash::engine::server::ReslistRoute route)
{
	using xash::engine::server::ReslistRoute;

	switch (route)
	{
	case ReslistRoute::SoundIndex:
		return SV_RESLIST_ROUTE_SOUND_INDEX;
	case ReslistRoute::GenericIndex:
		return SV_RESLIST_ROUTE_GENERIC_INDEX;
	case ReslistRoute::Skip:
	default:
		return SV_RESLIST_ROUTE_SKIP;
	}
}

}

extern "C" void SV_ResourceCatalog_Init(sv_resource_catalog_state_t *state)
{
	if (!state)
		return;

	state->sound_sentence_marker_added = 0;
}

extern "C" int SV_ResourceCatalog_NeedsFileSize(
	resourcetype_t type,
	const char *name)
{
	return xash::engine::server::ResourceCatalogNeedsFileSize(
		adapter::ToModernResourceType(type),
		name) ? 1 : 0;
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddGeneric(
	const char *name,
	int index,
	int probed_download_size)
{
	return ToLegacyCatalogEntry(
		xash::engine::server::BuildGenericResourceCatalogEntry(
			name,
			index,
			probed_download_size));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddSound(
	sv_resource_catalog_state_t *state,
	const char *name,
	int index,
	int probed_download_size)
{
	xash::engine::server::ResourceCatalogState modern =
		ToModernCatalogState(state);
	const xash::engine::server::ResourceCatalogEntry entry =
		xash::engine::server::BuildSoundResourceCatalogEntry(
			modern,
			name,
			index,
			probed_download_size);
	CopyModernCatalogState(state, modern);
	return ToLegacyCatalogEntry(entry);
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddModel(
	const char *name,
	int index,
	int probed_download_size,
	unsigned char flags)
{
	return ToLegacyCatalogEntry(
		xash::engine::server::BuildModelResourceCatalogEntry(
			name,
			index,
			probed_download_size,
			flags));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddDecal(
	const char *name,
	int index)
{
	return ToLegacyCatalogEntry(
		xash::engine::server::BuildDecalResourceCatalogEntry(name, index));
}

extern "C" sv_resource_catalog_entry_t SV_ResourceCatalog_AddEventScript(
	const char *name,
	int index,
	int probed_download_size)
{
	return ToLegacyCatalogEntry(
		xash::engine::server::BuildEventScriptResourceCatalogEntry(
			name,
			index,
			probed_download_size));
}

extern "C" int SV_ServerDownloadPolicy_NeedsModelTextureProbe(
	const char *requested_name,
	int allow_download,
	int send_resources,
	const resource_t *resources,
	int resource_count)
{
	std::vector<xash::engine::server::ResourceDescriptor> resourceSnapshot =
		adapter::BuildResourceDescriptorSnapshot(resources, resource_count);

	xash::engine::server::ServerDownloadRequest request = {};
	request.requestedName = requested_name;
	request.allowDownload = allow_download != 0;
	request.sendResources = send_resources != 0;
	request.resources =
		resourceSnapshot.empty() ? nullptr : resourceSnapshot.data();
	request.resourceCount = resourceSnapshot.size();

	return xash::engine::server::ServerDownloadNeedsModelTextureProbe(
		request) ? 1 : 0;
}

extern "C" sv_download_policy_decision_t SV_ServerDownloadPolicy_Decide(
	const char *requested_name,
	int allow_download,
	int send_resources,
	int send_logos,
	const resource_t *resources,
	int resource_count,
	const char *model_texture_name,
	int model_texture_available)
{
	std::vector<xash::engine::server::ResourceDescriptor> resourceSnapshot =
		adapter::BuildResourceDescriptorSnapshot(resources, resource_count);

	xash::engine::server::ServerDownloadRequest request = {};
	request.requestedName = requested_name;
	request.allowDownload = allow_download != 0;
	request.sendResources = send_resources != 0;
	request.sendLogos = send_logos != 0;
	request.resources =
		resourceSnapshot.empty() ? nullptr : resourceSnapshot.data();
	request.resourceCount = resourceSnapshot.size();
	request.modelTextureName = model_texture_name;
	request.modelTextureAvailable = model_texture_available != 0;

	return ToLegacyDownloadDecision(
		xash::engine::server::DecideServerDownload(request));
}

extern "C" int SV_UploadQueue_IsClientResourceDescriptorValid(
	const resource_t *resource)
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

extern "C" int SV_UploadQueue_ShouldEstimateUploadNeed(
	const resource_t *resource)
{
	return xash::engine::server::ResourceShouldEstimateUploadNeed(
		adapter::ToModernResourceDescriptor(resource)) ? 1 : 0;
}

extern "C" sv_upload_estimate_decision_t
SV_UploadQueue_DecideUploadEstimate(
	const resource_t *resource,
	int hpak_contains_resource)
{
	return ToLegacyUploadEstimateDecision(
		xash::engine::server::DecideUploadEstimate(
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

extern "C" int SV_UploadQueue_BatchNeedsCustomDataProbe(
	const resource_t *resource)
{
	return xash::engine::server::UploadBatchNeedsCustomDataProbe(
		adapter::ToModernResourceDescriptor(resource)) ? 1 : 0;
}

extern "C" enum sv_upload_batch_action_e SV_UploadQueue_DecideBatchAction(
	const resource_t *resource,
	int custom_resource_data_exists,
	int allow_upload)
{
	return ToLegacyUploadBatchAction(
		xash::engine::server::DecideUploadBatchAction(
			adapter::ToModernResourceDescriptor(resource),
			custom_resource_data_exists != 0,
			allow_upload != 0));
}

extern "C" sv_hot_resource_file_size_query_t
SV_HotResource_BuildFileSizeQuery(
	const char *name,
	resourcetype_t type)
{
	const xash::engine::server::HotResourceFileSizeQuery modern =
		xash::engine::server::BuildHotResourceFileSizeQuery(
			ToModernHotResourceRequest(name, type, 0, 0));

	sv_hot_resource_file_size_query_t legacy = {};
	legacy.should_announce = modern.shouldAnnounce ? 1 : 0;
	legacy.needs_file_size = modern.needsFileSize ? 1 : 0;

	if (!modern.path.empty())
	{
		adapter::CopyStringToLegacyBuffer(
			legacy.file_size_path,
			sizeof(legacy.file_size_path),
			modern.path);
	}

	return legacy;
}

extern "C" sv_hot_resource_entry_t SV_HotResource_BuildAnnouncement(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags,
	int probed_download_size)
{
	const xash::engine::server::HotResourceAnnouncement modern =
		xash::engine::server::BuildHotResourceAnnouncement(
			ToModernHotResourceRequest(name, type, index, flags),
			probed_download_size);
	const adapter::LegacyResourceDescriptorFields fields =
		adapter::ToLegacyResourceDescriptorFields(modern.resource, type);

	sv_hot_resource_entry_t legacy = {};
	legacy.should_announce = modern.shouldAnnounce ? 1 : 0;
	legacy.type = fields.type;
	legacy.name = fields.name;
	legacy.index = fields.index;
	legacy.download_size = fields.downloadSize;
	legacy.flags = fields.flags;
	return legacy;
}

extern "C" sv_reslist_decision_t SV_ReslistPolicy_ClassifyToken(
	const char *token)
{
	const xash::engine::server::ReslistTokenProbe probe =
		xash::engine::server::BuildReslistTokenProbe(token);
	const bool soundSupported =
		probe.soundPathCandidate &&
		Sound_SupportedFileFormat(
			xash::utilities::FileExtension(probe.normalizedPath.c_str()));
	const xash::engine::server::ReslistTokenDecision modern =
		xash::engine::server::BuildReslistTokenDecision(
			probe,
			soundSupported);

	sv_reslist_decision_t legacy = {};
	legacy.should_index = modern.shouldIndex ? 1 : 0;
	legacy.type = adapter::ToLegacyResourceType(modern.type);
	legacy.route = ToLegacyReslistRoute(modern.route);
	adapter::CopyStringToLegacyBuffer(
		legacy.normalized_path,
		sizeof(legacy.normalized_path),
		modern.normalizedPath);
	adapter::CopyStringToLegacyBuffer(
		legacy.index_path,
		sizeof(legacy.index_path),
		modern.indexPath);
	return legacy;
}
