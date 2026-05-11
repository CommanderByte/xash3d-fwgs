#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/game_dll/game_dll_resource_policy.hpp"
#include "engine/server/resources/resource_identity.hpp"
#include "engine/server/resources/resource_transfer_manifest.hpp"
#include "engine/server/resources/server_consistency_list.hpp"
#include "engine/server/resources/server_consistency_policy.hpp"
#include "engine/server/messaging/server_customization_message.hpp"
#include "engine/server/resources/server_download_policy.hpp"
#include "engine/server/resources/server_hot_resource.hpp"
#include "engine/server/resources/server_reslist_policy.hpp"
#include "engine/server/resources/server_resource_catalog.hpp"
#include "engine/server/messaging/server_resource_message.hpp"
#include "engine/server/resources/server_upload_queue.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

namespace
{

static bool ReadString(NetworkBitBuffer &reader, char *dst, std::size_t capacity)
{
	if (!dst || capacity == 0)
		return false;

	for (std::size_t i = 0; i < capacity; ++i)
	{
		dst[i] = static_cast<char>(reader.readUnsigned(8));
		if (dst[i] == '\0')
			return true;
	}

	dst[capacity - 1] = '\0';
	return false;
}

static void FillHash(std::uint8_t *hash, std::size_t size, std::uint8_t base)
{
	for (std::size_t i = 0; i < size; ++i)
		hash[i] = static_cast<std::uint8_t>(base + i);
}

static bool TestPrecacheReslistHotResourceAndDownloadFlow()
{
	const GameDllResourceNameDecision modelName =
		BuildGameDllResourceNameDecision(
			"!models\\barney.mdl",
			GameDllResourceNameMode::ModelPrecache,
			64);
	const GameDllModelPrecacheLoadAction modelLoad =
		BuildGameDllModelPrecacheLoadAction(modelName.optional, 2);
	const ReslistTokenDecision soundToken =
		ClassifyReslistToken("sound//items/suitchargeok1.wav", true);

	if (modelName.action != GameDllResourceNameAction::UseName ||
		!modelName.optional ||
		modelName.normalizedName != "models/barney.mdl" ||
		modelLoad != GameDllModelPrecacheLoadAction::LoadOptional ||
		BuildGameDllResourceSlotAction(3, 4) != GameDllResourceSlotAction::UseSlot ||
		!soundToken.shouldIndex ||
		soundToken.type != ResourceType::Sound ||
		soundToken.indexPath != "items/suitchargeok1.wav")
	{
		return false;
	}

	const HotResourceRequest hotSound =
	{
		ResourceType::Sound,
		soundToken.indexPath.c_str(),
		9,
		0
	};
	const HotResourceFileSizeQuery hotQuery =
		BuildHotResourceFileSizeQuery(hotSound);
	const HotResourceAnnouncement hotAnnouncement =
		BuildHotResourceAnnouncement(hotSound, 456);

	if (!hotQuery.shouldAnnounce ||
		!hotQuery.needsFileSize ||
		hotQuery.path != "sound/items/suitchargeok1.wav" ||
		!hotAnnouncement.shouldAnnounce ||
		hotAnnouncement.resource.type != ResourceType::Sound ||
		hotAnnouncement.resource.downloadSize != 456)
	{
		return false;
	}

	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);

	ResourceTransferManifest manifest;
	manifest.append(BuildGenericResourceCatalogEntry("sprites/hud640.spr", 3, 77));
	manifest.append(BuildSoundResourceCatalogEntry(
		state,
		soundToken.indexPath.c_str(),
		4,
		99));
	manifest.append(BuildModelResourceCatalogEntry(
		modelName.normalizedName.c_str(),
		7,
		321,
		kResourceCatalogFlagFatalIfMissing));
	manifest.append(hotAnnouncement.resource);

	const ResourceSizeSummary summary = manifest.sizeSummary();
	if (manifest.size() != 4 ||
		summary.totalSize != 953 ||
		summary.sizeByType[static_cast<int>(ResourceType::Sound)] != 555 ||
		manifest.findDownloadIndex("sound/items/suitchargeok1.wav") != 1 ||
		manifest.findDownloadIndex("models/barney.mdl") != 2 ||
		!ResourceMatchesDownloadName(
			*manifest.resourceAt(1),
			"xxxxxxitems/suitchargeok1.wav"))
	{
		return false;
	}

	const ServerDownloadRequest modelProbe =
		manifest.buildDownloadRequest("models/barney.mdl", true, true, true);
	const ServerDownloadDecision soundDownload =
		DecideServerDownload(manifest.buildDownloadRequest(
			"sound/items/suitchargeok1.wav",
			true,
			true,
			true));
	const ServerDownloadDecision modelDownload =
		DecideServerDownload(manifest.buildDownloadRequest(
			"models/barney.mdl",
			true,
			true,
			true,
			"models/barneyT.mdl",
			true));
	const ServerDownloadDecision disabled =
		DecideServerDownload(manifest.buildDownloadRequest(
			"sprites/hud640.spr",
			true,
			false,
			true));

	return ServerDownloadNeedsModelTextureProbe(modelProbe) &&
		soundDownload.action == ServerDownloadAction::SendFile &&
		soundDownload.resourceIndex == 1 &&
		modelDownload.action == ServerDownloadAction::SendFileWithModelTexture &&
		modelDownload.resourceIndex == 2 &&
		std::strcmp(modelDownload.modelTextureName, "models/barneyT.mdl") == 0 &&
		disabled.action == ServerDownloadAction::Reject;
}

static bool TestConsistencyReservedDataFeedsMessageAndListWriters()
{
	const float mins[] = { -16.0f, -16.0f, -36.0f };
	const float maxs[] = { 16.0f, 16.0f, 36.0f };
	const float insideMins[] = { -12.0f, -12.0f, -32.0f };
	const float insideMaxs[] = { 12.0f, 12.0f, 32.0f };
	std::uint8_t hash[kResourceHashSize] = {};
	std::uint8_t hashPrefix[kConsistencyPolicyHashPrefixSize] = {};

	FillHash(hash, sizeof(hash), 0x40);
	std::memcpy(hashPrefix, hash, sizeof(hashPrefix));

	ConsistencyReservationRequest request = {};
	request.resourceType = ResourceType::Model;
	request.forceType = kConsistencyForceModelSpecifyBounds;
	request.specifiedMins = mins;
	request.specifiedMaxs = maxs;

	const ConsistencyReservationResult reservation =
		BuildConsistencyReservation(request);
	if (!ConsistencyResourceNeedsSetup(false, true) ||
		!reservation.hasReservedData ||
		reservation.unsupportedForceType ||
		EvaluateConsistencyChecksum(7, hash, hashPrefix, sizeof(hashPrefix)).status !=
			ConsistencyEntryStatus::Match ||
		EvaluateConsistencyBounds(
			7,
			reservation.reservedData,
			insideMins,
			insideMaxs).status != ConsistencyEntryStatus::Match)
	{
		return false;
	}

	ResourceDescriptor model = {};
	model.name = "models/barney.mdl";
	model.type = ResourceType::Model;
	model.index = 7;
	model.downloadSize = 321;
	model.flags = kResourceMessageFlagFatalIfMissing;
	model.md5Hash = hash;

	ResourceTransferManifest manifest;
	ResourceMessageRow row = {};
	if (!manifest.append(model) ||
		!manifest.buildResourceMessageRow(0, row, reservation.reservedData) ||
		!ResourceMessageRowHasReservedData(row))
	{
		return false;
	}

	unsigned char rowData[256] = {};
	NetworkBitBuffer rowWriter(rowData, sizeof(rowData) << 3);
	WriteResourceMessageRow(rowWriter, row);

	NetworkBitBuffer rowReader(rowData, rowWriter.tellBit());
	char name[64] = {};
	std::uint8_t readReserved[kResourceMessageReservedSize] = {};

	if (rowReader.readUnsigned(4) != static_cast<unsigned int>(ResourceType::Model) ||
		!ReadString(rowReader, name, sizeof(name)) ||
		std::strcmp(name, "models/barney.mdl") != 0 ||
		rowReader.readUnsigned(kResourceMessageModelIndexBits) != 7 ||
		rowReader.readSigned(kResourceMessageDownloadSizeBits) != 321 ||
		rowReader.readUnsigned(kResourceMessageFlagsBits) !=
			kResourceMessageFlagFatalIfMissing ||
		rowReader.readOneBit() != 1 ||
		!rowReader.readBits(readReserved, kResourceMessageReservedSize << 3) ||
		std::memcmp(readReserved, reservation.reservedData, sizeof(readReserved)) != 0)
	{
		return false;
	}

	unsigned char listData[32] = {};
	int indexes[] = { 7, 11 };
	ConsistencyListRequest listRequest = {};
	listRequest.maxClients = 2;
	listRequest.consistencyEnabled = true;
	listRequest.consistencyCount = 2;
	listRequest.hltvProxy = false;
	listRequest.resourceIndexes = indexes;
	listRequest.resourceIndexCount = 2;

	NetworkBitBuffer listWriter(listData, sizeof(listData) << 3);
	const ConsistencyListWriteResult listResult =
		WriteConsistencyList(listWriter, listRequest);

	NetworkBitBuffer listReader(listData, listWriter.tellBit());
	int lastCheck = 0;
	int readIndexes[2] = {};
	std::size_t readCount = 0;

	if (!listResult.forceUnmodified ||
		listWriter.overflow() ||
		listReader.readOneBit() != 1)
	{
		return false;
	}

	while (listReader.readOneBit())
	{
		if (readCount >= 2)
			return false;

		const bool isDelta = listReader.readOneBit() != 0;
		const int index = isDelta ?
			static_cast<int>(listReader.readUnsigned(kConsistencyListDeltaBits)) +
				lastCheck :
			static_cast<int>(listReader.readUnsigned(kConsistencyListResourceIndexBits));

		readIndexes[readCount++] = index;
		lastCheck = index;
	}

	return readCount == 2 &&
		readIndexes[0] == 7 &&
		readIndexes[1] == 11 &&
		listReader.tellBit() == listWriter.tellBit() &&
		!rowReader.overflow() &&
		!rowWriter.overflow() &&
		!listReader.overflow();
}

static bool TestUploadLogoAndCustomizationPayloadFlow()
{
	std::uint8_t hash[kResourceHashSize] = {};
	FillHash(hash, sizeof(hash), 0x10);

	ResourceDescriptor custom = {};
	custom.name = "custom.hpk";
	custom.type = ResourceType::Decal;
	custom.index = 5;
	custom.downloadSize = 256;
	custom.flags = kUploadResourceFlagWasMissing | kUploadResourceFlagCustom;
	custom.md5Hash = hash;

	const UploadEstimateDecision estimate =
		DecideUploadEstimate(custom, false);
	const UploadBatchAction requestUpload =
		DecideUploadBatchAction(custom, false, true);
	const UploadBatchAction alreadyAvailable =
		DecideUploadBatchAction(custom, true, true);

	if (!ClientUploadResourceDescriptorIsValid(custom) ||
		!ResourceShouldEstimateUploadNeed(custom) ||
		estimate.action != UploadEstimateAction::MarkMissing ||
		estimate.uploadSize != 256 ||
		requestUpload != UploadBatchAction::RequestCustomUpload ||
		alreadyAvailable != UploadBatchAction::MoveToOnHand ||
		UploadTotalExceedsLimit(256, 1.0) ||
		!UploadTotalExceedsLimit(2 * 1024 * 1024, 1.0))
	{
		return false;
	}

	char customName[kCustomMd5ResourceNameLength + 1] = {};
	std::uint8_t parsedHash[kResourceHashSize] = {};
	if (!FormatCustomMd5ResourceName(customName, sizeof(customName), hash) ||
		!IsCustomMd5ResourceName(customName) ||
		!ParseCustomMd5ResourceName(customName, parsedHash) ||
		std::memcmp(parsedHash, hash, sizeof(hash)) != 0)
	{
		return false;
	}

	ServerDownloadRequest logoRequest = {};
	logoRequest.requestedName = customName;
	logoRequest.allowDownload = true;
	logoRequest.sendResources = true;
	logoRequest.sendLogos = true;

	const ServerDownloadDecision logo = DecideServerDownload(logoRequest);
	if (logo.action != ServerDownloadAction::LookupCustomLogo ||
		std::memcmp(logo.customHash, hash, sizeof(hash)) != 0)
	{
		return false;
	}

	CustomizationMessage customization = {};
	customization.playerNumber = 2;
	customization.type = custom.type;
	customization.name = custom.name;
	customization.index = custom.index;
	customization.downloadSize = custom.downloadSize;
	customization.flags = kCustomizationMessageFlagCustom;
	customization.md5Hash = custom.md5Hash;

	unsigned char payload[128] = {};
	NetworkBitBuffer writer(payload, sizeof(payload) << 3);
	WriteCustomizationMessagePayload(writer, customization);

	NetworkBitBuffer reader(payload, writer.tellBit());
	char readName[32] = {};
	std::uint8_t readHash[kCustomizationMessageHashSize] = {};

	return reader.readUnsigned(8) == 2 &&
		reader.readUnsigned(8) == static_cast<unsigned int>(ResourceType::Decal) &&
		ReadString(reader, readName, sizeof(readName)) &&
		std::strcmp(readName, "custom.hpk") == 0 &&
		reader.readSigned(16) == 5 &&
		reader.readSigned(32) == 256 &&
		reader.readUnsigned(8) == kCustomizationMessageFlagCustom &&
		reader.readBits(readHash, kCustomizationMessageHashSize << 3) &&
		std::memcmp(readHash, hash, sizeof(readHash)) == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

}

int main()
{
	if (!TestPrecacheReslistHotResourceAndDownloadFlow() ||
		!TestConsistencyReservedDataFeedsMessageAndListWriters() ||
		!TestUploadLogoAndCustomizationPayloadFlow())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
