#include <cstdlib>
#include <cstring>

#include "engine/server/resources/server_download_policy.hpp"
#include "engine/server/resources/server_hot_resource.hpp"
#include "engine/server/resources/server_reslist_policy.hpp"
#include "engine/server/resources/server_resource_catalog.hpp"
#include "engine/server/resources/server_upload_queue.hpp"

using namespace xash::engine::server;

namespace
{

static bool CatalogEntryMatches(
	const ResourceCatalogEntry &entry,
	ResourceType type,
	const char *name,
	int index,
	int downloadSize,
	unsigned int flags)
{
	return entry.shouldAdd &&
		entry.resource.type == type &&
		std::strcmp(entry.resource.name, name) == 0 &&
		entry.resource.index == index &&
		entry.resource.downloadSize == downloadSize &&
		entry.resource.flags == flags;
}

static ResourceDescriptor DownloadResource(const char *name, ResourceType type)
{
	ResourceDescriptor resource = {};
	resource.name = name;
	resource.type = type;
	return resource;
}

static ServerDownloadDecision DecideDownload(
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

static bool DownloadActionIs(
	const ServerDownloadDecision &decision,
	ServerDownloadAction action)
{
	return decision.action == action;
}

static ResourceDescriptor UploadResource(
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

static HotResourceRequest HotRequest(
	ResourceType type,
	const char *name,
	int index,
	unsigned int flags)
{
	HotResourceRequest request = {};
	request.type = type;
	request.name = name;
	request.index = index;
	request.flags = flags;
	return request;
}

static bool TestCatalogEmptyEntriesAreSkipped()
{
	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);

	return !ResourceCatalogNeedsFileSize(ResourceType::Generic, "") &&
		!ResourceCatalogNeedsFileSize(ResourceType::Model, nullptr) &&
		!BuildGenericResourceCatalogEntry("", 1, 10).shouldAdd &&
		!BuildSoundResourceCatalogEntry(state, nullptr, 1, 10).shouldAdd &&
		!BuildModelResourceCatalogEntry("", 1, 10, 0).shouldAdd &&
		!BuildDecalResourceCatalogEntry(nullptr, 1).shouldAdd &&
		!BuildEventScriptResourceCatalogEntry("", 1, 10).shouldAdd;
}

static bool TestCatalogGenericAndEventEntries()
{
	const ResourceCatalogEntry generic =
		BuildGenericResourceCatalogEntry("maps/test.res", 3, 44);
	const ResourceCatalogEntry event =
		BuildEventScriptResourceCatalogEntry("events/test.sc", 9, 55);

	return ResourceCatalogNeedsFileSize(
			ResourceType::Generic,
			"maps/test.res") &&
		ResourceCatalogNeedsFileSize(
			ResourceType::EventScript,
			"events/test.sc") &&
		CatalogEntryMatches(
			generic,
			ResourceType::Generic,
			"maps/test.res",
			3,
			44,
			kResourceCatalogFlagFatalIfMissing) &&
		CatalogEntryMatches(
			event,
			ResourceType::EventScript,
			"events/test.sc",
			9,
			55,
			kResourceCatalogFlagFatalIfMissing);
}

static bool TestCatalogSoundSentenceMarkerIsAddedOnce()
{
	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);

	if (ResourceCatalogNeedsFileSize(ResourceType::Sound, "!HEV_AMO0"))
		return false;

	const ResourceCatalogEntry first =
		BuildSoundResourceCatalogEntry(state, "!HEV_AMO0", 12, 999);
	const ResourceCatalogEntry second =
		BuildSoundResourceCatalogEntry(state, "!HEV_AMO1", 13, 888);
	const ResourceCatalogEntry regular =
		BuildSoundResourceCatalogEntry(state, "weapons/test.wav", 14, 777);

	if (!state.soundSentenceMarkerAdded ||
		!CatalogEntryMatches(
			first,
			ResourceType::Sound,
			"!",
			12,
			0,
			kResourceCatalogFlagFatalIfMissing) ||
		second.shouldAdd)
	{
		return false;
	}

	return ResourceCatalogNeedsFileSize(
			ResourceType::Sound,
			"weapons/test.wav") &&
		CatalogEntryMatches(
			regular,
			ResourceType::Sound,
			"weapons/test.wav",
			14,
			777,
			0);
}

static bool TestCatalogSoundStateResetAllowsNewMarker()
{
	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);
	BuildSoundResourceCatalogEntry(state, "!FIRST", 1, 0);
	ResetResourceCatalogState(state);

	const ResourceCatalogEntry entry =
		BuildSoundResourceCatalogEntry(state, "!SECOND", 2, 0);

	return CatalogEntryMatches(
		entry,
		ResourceType::Sound,
		"!",
		2,
		0,
		kResourceCatalogFlagFatalIfMissing);
}

static bool TestCatalogModelSizeAndFlags()
{
	const ResourceCatalogEntry external =
		BuildModelResourceCatalogEntry("models/player.mdl", 4, 1000, 5);
	const ResourceCatalogEntry inlineModel =
		BuildModelResourceCatalogEntry("*12", 5, 1000, 7);

	return ResourceCatalogNeedsFileSize(
			ResourceType::Model,
			"models/player.mdl") &&
		!ResourceCatalogNeedsFileSize(ResourceType::Model, "*12") &&
		CatalogEntryMatches(
			external,
			ResourceType::Model,
			"models/player.mdl",
			4,
			1000,
			5) &&
		CatalogEntryMatches(
			inlineModel,
			ResourceType::Model,
			"*12",
			5,
			0,
			7);
}

static bool TestCatalogDecalEntry()
{
	const ResourceCatalogEntry decal =
		BuildDecalResourceCatalogEntry("{shot1", 0);

	return !ResourceCatalogNeedsFileSize(ResourceType::Decal, "{shot1") &&
		CatalogEntryMatches(decal, ResourceType::Decal, "{shot1", 0, 0, 0);
}

static bool TestDownloadIgnoreAndReject()
{
	ResourceDescriptor resources[] =
	{
		DownloadResource("models/player.mdl", ResourceType::Model),
	};

	return DownloadActionIs(
			DecideDownload(nullptr),
			ServerDownloadAction::Ignore) &&
		DownloadActionIs(DecideDownload(""), ServerDownloadAction::Ignore) &&
		DownloadActionIs(
			DecideDownload("../valve/resource/GameMenu.res"),
			ServerDownloadAction::Reject) &&
		DownloadActionIs(
			DecideDownload(
				"models/player.mdl",
				false,
				true,
				true,
				resources,
				1),
			ServerDownloadAction::Reject) &&
		DownloadActionIs(
			DecideDownload(
				"models/player.mdl",
				true,
				false,
				true,
				resources,
				1),
			ServerDownloadAction::Reject) &&
		DownloadActionIs(
			DecideDownload(
				"models/missing.mdl",
				true,
				true,
				true,
				resources,
				1),
			ServerDownloadAction::Reject);
}

static bool TestRegularDownloads()
{
	ResourceDescriptor resources[] =
	{
		DownloadResource("player/pl_wade1.wav", ResourceType::Sound),
		DownloadResource("models/player.mdl", ResourceType::Model),
		DownloadResource("sprites/hud.txt", ResourceType::Generic),
	};

	const ServerDownloadDecision sound = DecideDownload(
		"sound/player/pl_wade1.wav",
		true,
		true,
		true,
		resources,
		3);

	const ServerDownloadDecision blindSound = DecideDownload(
		"xxxxxxplayer/pl_wade1.wav",
		true,
		true,
		true,
		resources,
		3);

	const ServerDownloadDecision model = DecideDownload(
		"models/player.mdl",
		true,
		true,
		true,
		resources,
		3);

	const ServerDownloadDecision modelWithTexture = DecideDownload(
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
		modelWithTexture.action ==
			ServerDownloadAction::SendFileWithModelTexture &&
		modelWithTexture.resourceIndex == 1 &&
		std::strcmp(modelWithTexture.modelTextureName, "models/playerT.mdl") ==
			0;
}

static bool TestModelTextureProbe()
{
	ResourceDescriptor resources[] =
	{
		DownloadResource("models/player.mdl", ResourceType::Model),
		DownloadResource("sprites/hud.txt", ResourceType::Generic),
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
	const ServerDownloadDecision logo = DecideDownload(name);

	return logo.action == ServerDownloadAction::LookupCustomLogo &&
		logo.customHash[0] == 0x01 &&
		logo.customHash[1] == 0x23 &&
		logo.customHash[15] == 0xEF &&
		DownloadActionIs(
			DecideDownload(name, true, true, false),
			ServerDownloadAction::Reject) &&
		DownloadActionIs(
			DecideDownload("!MD50123456789abcdef0123456789ABCDEF"),
			ServerDownloadAction::Reject) &&
		DownloadActionIs(
			DecideDownload("!notmd5.wad"),
			ServerDownloadAction::Reject);
}

static bool TestUploadDescriptorValidation()
{
	return ClientUploadResourceDescriptorIsValid(
			UploadResource(ResourceType::World, kMaxClientResourceUploadSize)) &&
		ClientUploadResourceDescriptorIsValid(
			UploadResource(ResourceType::Decal, -1)) &&
		!ClientUploadResourceDescriptorIsValid(
			UploadResource(ResourceType::Unknown, 1)) &&
		!ClientUploadResourceDescriptorIsValid(
			UploadResource(ResourceType::Generic,
				kMaxClientResourceUploadSize + 1));
}

static bool TestResourceListTiming()
{
	return ResourceListUpdateIsTooSoon(9.99, 10.0) &&
		!ResourceListUpdateIsTooSoon(10.0, 10.0) &&
		!ResourceListUpdateIsTooSoon(10.01, 10.0);
}

static bool TestUploadEstimate()
{
	const UploadEstimateDecision ignored =
		DecideUploadEstimate(UploadResource(ResourceType::Model, 100), false);
	const UploadEstimateDecision existing =
		DecideUploadEstimate(UploadResource(ResourceType::Decal, 100), true);
	const UploadEstimateDecision missing =
		DecideUploadEstimate(UploadResource(ResourceType::Decal, 100), false);
	const UploadEstimateDecision zero =
		DecideUploadEstimate(UploadResource(ResourceType::Decal, 0), false);

	return !ResourceShouldEstimateUploadNeed(
			UploadResource(ResourceType::Sound, 100)) &&
		ResourceShouldEstimateUploadNeed(
			UploadResource(ResourceType::Decal, 100)) &&
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

static bool TestUploadBatchActions()
{
	const ResourceDescriptor onHand = UploadResource(ResourceType::Model, 100, 0);
	const ResourceDescriptor customMissing = UploadResource(
		ResourceType::Decal,
		100,
		kUploadResourceFlagWasMissing | kUploadResourceFlagCustom);
	const ResourceDescriptor nonCustomMissing = UploadResource(
		ResourceType::Decal,
		100,
		kUploadResourceFlagWasMissing);
	const ResourceDescriptor impossibleMissingModel = UploadResource(
		ResourceType::Model,
		100,
		kUploadResourceFlagWasMissing);

	return !UploadBatchNeedsCustomDataProbe(onHand) &&
		UploadBatchNeedsCustomDataProbe(customMissing) &&
		DecideUploadBatchAction(onHand, false, true) ==
			UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, true, true) ==
			UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, false, false) ==
			UploadBatchAction::MoveToOnHand &&
		DecideUploadBatchAction(customMissing, false, true) ==
			UploadBatchAction::RequestCustomUpload &&
		DecideUploadBatchAction(nonCustomMissing, false, true) ==
			UploadBatchAction::ReportNonCustomAndMove &&
		DecideUploadBatchAction(impossibleMissingModel, false, true) ==
			UploadBatchAction::KeepInNeeded;
}

static bool TestHotEmptyNamesAreSkipped()
{
	const HotResourceRequest empty = HotRequest(ResourceType::Generic, "", 1, 0);
	const HotResourceRequest nullName =
		HotRequest(ResourceType::Model, nullptr, 1, 0);

	return !BuildHotResourceFileSizeQuery(empty).shouldAnnounce &&
		!BuildHotResourceAnnouncement(empty, 123).shouldAnnounce &&
		!BuildHotResourceFileSizeQuery(nullName).shouldAnnounce &&
		!BuildHotResourceAnnouncement(nullName, 123).shouldAnnounce;
}

static bool TestHotModelWildcardSkipsFileSizeButStillAnnounces()
{
	const HotResourceRequest request =
		HotRequest(ResourceType::Model, "*12", 7, 5);
	const HotResourceFileSizeQuery query =
		BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 999);

	return query.shouldAnnounce &&
		!query.needsFileSize &&
		query.path.empty() &&
		announcement.shouldAnnounce &&
		announcement.resource.type == ResourceType::Model &&
		std::strcmp(announcement.resource.name, "*12") == 0 &&
		announcement.resource.index == 7 &&
		announcement.resource.downloadSize == 0 &&
		announcement.resource.flags == 5;
}

static bool TestHotModelFileSizePathUsesResourceName()
{
	const HotResourceRequest request =
		HotRequest(ResourceType::Model, "models/w_test.mdl", 37, 1);
	const HotResourceFileSizeQuery query =
		BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 321);

	return query.shouldAnnounce &&
		query.needsFileSize &&
		query.path == "models/w_test.mdl" &&
		announcement.shouldAnnounce &&
		announcement.resource.downloadSize == 321 &&
		announcement.resource.flags == 1;
}

static bool TestHotSoundFileSizePathAddsLegacyPrefix()
{
	const HotResourceRequest request =
		HotRequest(ResourceType::Sound, "weapons/pl_gun3.wav", 9, 0);
	const HotResourceFileSizeQuery query =
		BuildHotResourceFileSizeQuery(request);
	const HotResourceAnnouncement announcement =
		BuildHotResourceAnnouncement(request, 456);

	return query.shouldAnnounce &&
		query.needsFileSize &&
		query.path == "sound/weapons/pl_gun3.wav" &&
		announcement.shouldAnnounce &&
		announcement.resource.type == ResourceType::Sound &&
		std::strcmp(announcement.resource.name, "weapons/pl_gun3.wav") == 0 &&
		announcement.resource.index == 9 &&
		announcement.resource.downloadSize == 456;
}

static bool TestHotGenericAndEventUseProvidedMetadata()
{
	const HotResourceRequest generic =
		HotRequest(ResourceType::Generic, "sprites/hud.txt", 12, 1);
	const HotResourceRequest event =
		HotRequest(ResourceType::EventScript, "events/test.sc", 13, 7);
	const HotResourceFileSizeQuery genericQuery =
		BuildHotResourceFileSizeQuery(generic);
	const HotResourceFileSizeQuery eventQuery =
		BuildHotResourceFileSizeQuery(event);
	const HotResourceAnnouncement genericAnnouncement =
		BuildHotResourceAnnouncement(generic, -42);
	const HotResourceAnnouncement eventAnnouncement =
		BuildHotResourceAnnouncement(event, 2048);

	return genericQuery.shouldAnnounce &&
		genericQuery.needsFileSize &&
		genericQuery.path == "sprites/hud.txt" &&
		eventQuery.shouldAnnounce &&
		eventQuery.needsFileSize &&
		eventQuery.path == "events/test.sc" &&
		genericAnnouncement.shouldAnnounce &&
		genericAnnouncement.resource.downloadSize == -42 &&
		genericAnnouncement.resource.flags == 1 &&
		eventAnnouncement.shouldAnnounce &&
		eventAnnouncement.resource.downloadSize == 2048 &&
		eventAnnouncement.resource.flags == 7;
}

static bool TestReslistUnsafeTokensAreSkipped()
{
	return !ClassifyReslistToken("", true).shouldIndex &&
		!ClassifyReslistToken(nullptr, true).shouldIndex &&
		!ClassifyReslistToken("../valve/gfx.wad", true).shouldIndex &&
		!ClassifyReslistToken("sound\\weapons\\bad.wav", true).shouldIndex &&
		!ClassifyReslistToken("scripts/autoexec.cfg", true).shouldIndex &&
		!ClassifyReslistToken("models/no_extension", true).shouldIndex;
}

static bool TestReslistSoundPathNormalizesAndRoutesToSoundIndex()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sound//weapons/pl_gun3.wav", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Sound &&
		decision.route == ReslistRoute::SoundIndex &&
		decision.normalizedPath == "sound/weapons/pl_gun3.wav" &&
		decision.indexPath == "weapons/pl_gun3.wav";
}

static bool TestUnsupportedSoundPathFallsBackToGeneric()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sound/ambience/test.xyz", false);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "sound/ambience/test.xyz" &&
		decision.indexPath == "sound/ambience/test.xyz";
}

static bool TestCaseSensitiveSoundPrefix()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("Sound/weapons/pl_gun3.wav", true);
	const ReslistTokenDecision mixed =
		ClassifyReslistToken("sOuNd/weapons/pl_gun3.wav", true);
	const ReslistTokenDecision adjacent =
		ClassifyReslistToken("soundx/weapons/pl_gun3.wav", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "Sound/weapons/pl_gun3.wav" &&
		decision.indexPath == "Sound/weapons/pl_gun3.wav" &&
		mixed.shouldIndex &&
		mixed.route == ReslistRoute::GenericIndex &&
		mixed.indexPath == "sOuNd/weapons/pl_gun3.wav" &&
		adjacent.shouldIndex &&
		adjacent.route == ReslistRoute::GenericIndex &&
		adjacent.indexPath == "soundx/weapons/pl_gun3.wav";
}

static bool TestReslistGenericFallback()
{
	const ReslistTokenDecision decision =
		ClassifyReslistToken("sprites/hud640.spr", true);

	return decision.shouldIndex &&
		decision.type == ResourceType::Generic &&
		decision.route == ReslistRoute::GenericIndex &&
		decision.normalizedPath == "sprites/hud640.spr" &&
		decision.indexPath == "sprites/hud640.spr";
}

static bool TestReslistProbeSeparatesSoundSupportCheck()
{
	const ReslistTokenProbe soundProbe =
		BuildReslistTokenProbe("sound/items/suitchargeok1.wav");
	const ReslistTokenProbe genericProbe =
		BuildReslistTokenProbe("models/w_battery.mdl");
	const ReslistTokenProbe unsafeProbe =
		BuildReslistTokenProbe("sound\\items\\bad.wav");

	return soundProbe.safe &&
		soundProbe.soundPathCandidate &&
		soundProbe.normalizedPath == "sound/items/suitchargeok1.wav" &&
		genericProbe.safe &&
		!genericProbe.soundPathCandidate &&
		genericProbe.normalizedPath == "models/w_battery.mdl" &&
		!unsafeProbe.safe;
}

static bool TestLegacySoundPathOddities()
{
	const ReslistTokenDecision dotted =
		ClassifyReslistToken("sound/.wav", true);
	const ReslistTokenDecision relative =
		ClassifyReslistToken("sound/./foo.wav", true);

	return !ClassifyReslistToken("sound/", true).shouldIndex &&
		!ClassifyReslistToken("sound/../foo.wav", true).shouldIndex &&
		dotted.shouldIndex &&
		dotted.type == ResourceType::Sound &&
		dotted.route == ReslistRoute::SoundIndex &&
		dotted.indexPath == ".wav" &&
		relative.shouldIndex &&
		relative.type == ResourceType::Sound &&
		relative.route == ReslistRoute::SoundIndex &&
		relative.indexPath == "./foo.wav";
}

}

int main()
{
	if (!TestCatalogEmptyEntriesAreSkipped() ||
		!TestCatalogGenericAndEventEntries() ||
		!TestCatalogSoundSentenceMarkerIsAddedOnce() ||
		!TestCatalogSoundStateResetAllowsNewMarker() ||
		!TestCatalogModelSizeAndFlags() ||
		!TestCatalogDecalEntry() ||
		!TestDownloadIgnoreAndReject() ||
		!TestRegularDownloads() ||
		!TestModelTextureProbe() ||
		!TestCustomLogoDownloads() ||
		!TestUploadDescriptorValidation() ||
		!TestResourceListTiming() ||
		!TestUploadEstimate() ||
		!TestUploadLimit() ||
		!TestUploadBatchActions() ||
		!TestHotEmptyNamesAreSkipped() ||
		!TestHotModelWildcardSkipsFileSizeButStillAnnounces() ||
		!TestHotModelFileSizePathUsesResourceName() ||
		!TestHotSoundFileSizePathAddsLegacyPrefix() ||
		!TestHotGenericAndEventUseProvidedMetadata() ||
		!TestReslistUnsafeTokensAreSkipped() ||
		!TestReslistSoundPathNormalizesAndRoutesToSoundIndex() ||
		!TestUnsupportedSoundPathFallsBackToGeneric() ||
		!TestCaseSensitiveSoundPrefix() ||
		!TestReslistGenericFallback() ||
		!TestReslistProbeSeparatesSoundSupportCheck() ||
		!TestLegacySoundPathOddities())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
