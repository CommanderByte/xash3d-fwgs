#include "server_download_policy_adapter.h"

#include "engine/server/resources/server_download_policy.hpp"
#include "resource_adapter_shared.hpp"

#include <cstring>
#include <vector>

namespace
{

sv_download_policy_decision_t ToLegacyDecision(
	const xash::engine::server::ServerDownloadDecision &decision)
{
	using xash::engine::server::ServerDownloadAction;

	sv_download_policy_decision_t legacy = {};
	legacy.resource_index = decision.resourceIndex;
	legacy.file_name = decision.fileName;
	legacy.model_texture_name = decision.modelTextureName;
	std::memcpy(legacy.custom_hash, decision.customHash, sizeof(legacy.custom_hash));

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

}

extern "C" int SV_ServerDownloadPolicy_NeedsModelTextureProbe(
	const char *requested_name,
	int allow_download,
	int send_resources,
	const resource_t *resources,
	int resource_count)
{
	std::vector<xash::engine::server::ResourceDescriptor> resourceSnapshot =
		xash::engine::server::adapter::BuildResourceDescriptorSnapshot(
			resources,
			resource_count);

	xash::engine::server::ServerDownloadRequest request = {};
	request.requestedName = requested_name;
	request.allowDownload = allow_download != 0;
	request.sendResources = send_resources != 0;
	request.resources = resourceSnapshot.empty() ? nullptr : resourceSnapshot.data();
	request.resourceCount = resourceSnapshot.size();

	return xash::engine::server::ServerDownloadNeedsModelTextureProbe(request) ? 1 : 0;
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
		xash::engine::server::adapter::BuildResourceDescriptorSnapshot(
			resources,
			resource_count);

	xash::engine::server::ServerDownloadRequest request = {};
	request.requestedName = requested_name;
	request.allowDownload = allow_download != 0;
	request.sendResources = send_resources != 0;
	request.sendLogos = send_logos != 0;
	request.resources = resourceSnapshot.empty() ? nullptr : resourceSnapshot.data();
	request.resourceCount = resourceSnapshot.size();
	request.modelTextureName = model_texture_name;
	request.modelTextureAvailable = model_texture_available != 0;

	return ToLegacyDecision(xash::engine::server::DecideServerDownload(request));
}
