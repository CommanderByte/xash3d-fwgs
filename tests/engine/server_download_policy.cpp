#include <cstdlib>
#include <cstring>

#include "engine/server/server_download_policy.hpp"

using namespace xash::engine::server;

static ResourceDescriptor Resource(const char *name, ResourceType type)
{
	ResourceDescriptor resource = {};
	resource.name = name;
	resource.type = type;
	return resource;
}

static ServerDownloadDecision Decide(
	const char *name,
	bool allowDownload = true,
	bool sendResources = true,
	bool sendLogos = true,
	const ResourceDescriptor *resources = nullptr,
	std::size_t resourceCount = 0,
	bool modelTextureAvailable = false,
	const char *modelTextureName = nullptr)
{
	ServerDownloadRequest request = {};
	request.requestedName = name;
	request.allowDownload = allowDownload;
	request.sendResources = sendResources;
	request.sendLogos = sendLogos;
	request.resources = resources;
	request.resourceCount = resourceCount;
	request.modelTextureAvailable = modelTextureAvailable;
	request.modelTextureName = modelTextureName;
	return DecideServerDownload(request);
}

static bool IsAction(const ServerDownloadDecision &decision, ServerDownloadAction action)
{
	return decision.action == action;
}

static bool TestIgnoreAndReject()
{
	ResourceDescriptor resources[] =
	{
		Resource("models/player.mdl", ResourceType::Model),
	};

	return IsAction(Decide(nullptr), ServerDownloadAction::Ignore) &&
		IsAction(Decide(""), ServerDownloadAction::Ignore) &&
		IsAction(Decide("../valve/resource/GameMenu.res"), ServerDownloadAction::Reject) &&
		IsAction(Decide("models/player.mdl", false, true, true, resources, 1), ServerDownloadAction::Reject) &&
		IsAction(Decide("models/player.mdl", true, false, true, resources, 1), ServerDownloadAction::Reject) &&
		IsAction(Decide("models/missing.mdl", true, true, true, resources, 1), ServerDownloadAction::Reject);
}

static bool TestRegularDownloads()
{
	ResourceDescriptor resources[] =
	{
		Resource("player/pl_wade1.wav", ResourceType::Sound),
		Resource("models/player.mdl", ResourceType::Model),
		Resource("sprites/hud.txt", ResourceType::Generic),
	};

	ServerDownloadDecision sound = Decide(
		"sound/player/pl_wade1.wav",
		true,
		true,
		true,
		resources,
		3);

	ServerDownloadDecision blindSound = Decide(
		"xxxxxxplayer/pl_wade1.wav",
		true,
		true,
		true,
		resources,
		3);

	ServerDownloadDecision model = Decide(
		"models/player.mdl",
		true,
		true,
		true,
		resources,
		3);

	ServerDownloadDecision modelWithTexture = Decide(
		"models/player.mdl",
		true,
		true,
		true,
		resources,
		3,
		true,
		"models/playerT.mdl");

	return sound.action == ServerDownloadAction::SendFile &&
		sound.resourceIndex == 0 &&
		blindSound.action == ServerDownloadAction::SendFile &&
		blindSound.resourceIndex == 0 &&
		model.action == ServerDownloadAction::SendFile &&
		model.resourceIndex == 1 &&
		modelWithTexture.action == ServerDownloadAction::SendFileWithModelTexture &&
		modelWithTexture.resourceIndex == 1 &&
		std::strcmp(modelWithTexture.modelTextureName, "models/playerT.mdl") == 0;
}

static bool TestModelTextureProbe()
{
	ResourceDescriptor resources[] =
	{
		Resource("models/player.mdl", ResourceType::Model),
		Resource("sprites/hud.txt", ResourceType::Generic),
	};

	ServerDownloadRequest request = {};
	request.requestedName = "models/player.mdl";
	request.allowDownload = true;
	request.sendResources = true;
	request.resources = resources;
	request.resourceCount = 2;

	ServerDownloadRequest missing = request;
	missing.requestedName = "models/missing.mdl";

	ServerDownloadRequest unsafe = request;
	unsafe.requestedName = "../models/player.mdl";

	ServerDownloadRequest disabled = request;
	disabled.allowDownload = false;

	ServerDownloadRequest resourcesDisabled = request;
	resourcesDisabled.sendResources = false;

	ServerDownloadRequest notModel = request;
	notModel.requestedName = "sprites/hud.txt";

	return ServerDownloadNeedsModelTextureProbe(request) &&
		!ServerDownloadNeedsModelTextureProbe(missing) &&
		!ServerDownloadNeedsModelTextureProbe(unsafe) &&
		!ServerDownloadNeedsModelTextureProbe(disabled) &&
		!ServerDownloadNeedsModelTextureProbe(resourcesDisabled) &&
		!ServerDownloadNeedsModelTextureProbe(notModel);
}

static bool TestCustomLogoDownloads()
{
	const char *name = "!MD50123456789ABCDEF0123456789ABCDEF";
	ServerDownloadDecision logo = Decide(name);

	return logo.action == ServerDownloadAction::LookupCustomLogo &&
		logo.customHash[0] == 0x01 &&
		logo.customHash[1] == 0x23 &&
		logo.customHash[15] == 0xEF &&
		IsAction(Decide(name, true, true, false), ServerDownloadAction::Reject) &&
		IsAction(Decide("!MD50123456789abcdef0123456789ABCDEF"), ServerDownloadAction::Reject) &&
		IsAction(Decide("!notmd5.wad"), ServerDownloadAction::Reject);
}

int main()
{
	if (!TestIgnoreAndReject() ||
		!TestRegularDownloads() ||
		!TestModelTextureProbe() ||
		!TestCustomLogoDownloads())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
