#include "server_download_policy_adapter.h"

#include "engine/server/server_download_policy.hpp"

#include <cstring>
#include <vector>

namespace
{

xash::engine::server::ResourceType ToModernResourceType(resourcetype_t type)
{
	using xash::engine::server::ResourceType;

	switch (type)
	{
	case t_sound:
		return ResourceType::Sound;
	case t_skin:
		return ResourceType::Skin;
	case t_model:
		return ResourceType::Model;
	case t_decal:
		return ResourceType::Decal;
	case t_generic:
		return ResourceType::Generic;
	case t_eventscript:
		return ResourceType::EventScript;
	case t_world:
		return ResourceType::World;
	default:
		return ResourceType::Unknown;
	}
}

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

std::vector<xash::engine::server::ResourceDescriptor> BuildResourceSnapshot(
	const resource_t *legacyResources,
	int resourceCount)
{
	std::vector<xash::engine::server::ResourceDescriptor> resources;

	if (!legacyResources || resourceCount <= 0)
		return resources;

	resources.reserve(static_cast<std::size_t>(resourceCount));

	for (int i = 0; i < resourceCount; ++i)
	{
		xash::engine::server::ResourceDescriptor resource = {};
		resource.name = legacyResources[i].szFileName;
		resource.type = ToModernResourceType(legacyResources[i].type);
		resource.index = legacyResources[i].nIndex;
		resource.downloadSize = legacyResources[i].nDownloadSize;
		resource.flags = legacyResources[i].ucFlags;
		resource.md5Hash = legacyResources[i].rgucMD5_hash;
		resources.push_back(resource);
	}

	return resources;
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
		BuildResourceSnapshot(resources, resource_count);

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
		BuildResourceSnapshot(resources, resource_count);

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
