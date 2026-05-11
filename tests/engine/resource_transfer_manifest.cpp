#include <cstdlib>
#include <cstring>

#include "engine/network/network_buffer.hpp"
#include "engine/server/resource_transfer_manifest.hpp"
#include "engine/server/server_reslist_policy.hpp"

using namespace xash::engine::network;
using namespace xash::engine::server;

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

static ResourceTransferManifest BuildCatalogManifest()
{
	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);

	const ResourceCatalogEntry entries[] =
	{
		BuildGenericResourceCatalogEntry("sprites/hud640.spr", 3, 77),
		BuildSoundResourceCatalogEntry(state, "weapons/pl_gun3.wav", 4, 99),
		BuildModelResourceCatalogEntry(
			"models/player.mdl",
			7,
			321,
			kResourceCatalogFlagFatalIfMissing),
		BuildEventScriptResourceCatalogEntry("events/test.sc", 8, 111),
		BuildGenericResourceCatalogEntry("", 9, 222),
	};

	ResourceTransferManifest manifest;
	const std::size_t added =
		manifest.appendAll(entries, sizeof(entries) / sizeof(entries[0]));

	if (added != 4)
		manifest.clear();

	return manifest;
}

static bool TestCatalogEntriesBuildDownloadManifest()
{
	ResourceTransferManifest manifest = BuildCatalogManifest();

	if (manifest.size() != 4 ||
		manifest.empty() ||
		!manifest.data() ||
		manifest.findDownloadIndex("sprites/hud640.spr") != 0 ||
		manifest.findDownloadIndex("sound/weapons/pl_gun3.wav") != 1 ||
		manifest.findDownloadIndex("models/player.mdl") != 2 ||
		manifest.findDownloadIndex("events/test.sc") != 3 ||
		manifest.findDownloadIndex("sound/missing.wav") != -1)
	{
		return false;
	}

	const ServerDownloadDecision sound = DecideServerDownload(
		manifest.buildDownloadRequest("sound/weapons/pl_gun3.wav", true, true, true));
	const ServerDownloadDecision model = DecideServerDownload(
		manifest.buildDownloadRequest(
			"models/player.mdl",
			true,
			true,
			true,
			"models/playerT.mdl",
			true));
	const ServerDownloadDecision disabled = DecideServerDownload(
		manifest.buildDownloadRequest("sprites/hud640.spr", true, false, true));

	return sound.action == ServerDownloadAction::SendFile &&
		sound.resourceIndex == 1 &&
		model.action == ServerDownloadAction::SendFileWithModelTexture &&
		model.resourceIndex == 2 &&
		std::strcmp(model.modelTextureName, "models/playerT.mdl") == 0 &&
		disabled.action == ServerDownloadAction::Reject;
}

static bool TestCatalogManifestSizeSummary()
{
	const ResourceTransferManifest manifest = BuildCatalogManifest();
	const ResourceSizeSummary summary = manifest.sizeSummary();

	return summary.totalSize == 608 &&
		summary.sizeByType[static_cast<int>(ResourceType::Sound)] == 99 &&
		summary.sizeByType[static_cast<int>(ResourceType::Model)] == 321 &&
		summary.sizeByType[static_cast<int>(ResourceType::Generic)] == 77 &&
		summary.sizeByType[static_cast<int>(ResourceType::EventScript)] == 111;
}

static bool TestCatalogManifestResourceMessageRow()
{
	ResourceTransferManifest manifest = BuildCatalogManifest();
	ResourceMessageRow row = {};

	if (!manifest.buildResourceMessageRow(2, row) ||
		row.type != ResourceType::Model ||
		std::strcmp(row.name, "models/player.mdl") != 0 ||
		row.index != 7 ||
		row.downloadSize != 321 ||
		row.flags != kResourceCatalogFlagFatalIfMissing)
	{
		return false;
	}

	unsigned char data[128] = {};
	NetworkBitBuffer writer(data, sizeof(data) << 3);
	WriteResourceMessageRow(writer, row);

	NetworkBitBuffer reader(data, writer.tellBit());
	char name[64] = {};

	return reader.readUnsigned(4) == static_cast<unsigned int>(ResourceType::Model) &&
		ReadString(reader, name, sizeof(name)) &&
		std::strcmp(name, "models/player.mdl") == 0 &&
		reader.readUnsigned(kResourceMessageModelIndexBits) == 7 &&
		reader.readSigned(kResourceMessageDownloadSizeBits) == 321 &&
		reader.readUnsigned(kResourceMessageFlagsBits) == kResourceMessageFlagFatalIfMissing &&
		reader.readOneBit() == 0 &&
		reader.tellBit() == writer.tellBit() &&
		!reader.overflow() &&
		!writer.overflow();
}

static bool TestReslistTokensCanFeedManifest()
{
	const ReslistTokenDecision sound =
		ClassifyReslistToken("sound//items/suitchargeok1.wav", true);
	const ReslistTokenDecision unsupportedSound =
		ClassifyReslistToken("sound/ambience/test.xyz", false);

	if (!sound.shouldIndex ||
		sound.route != ReslistRoute::SoundIndex ||
		sound.normalizedPath != "sound/items/suitchargeok1.wav" ||
		sound.indexPath != "items/suitchargeok1.wav" ||
		!unsupportedSound.shouldIndex ||
		unsupportedSound.route != ReslistRoute::GenericIndex)
	{
		return false;
	}

	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);

	ResourceTransferManifest manifest;
	manifest.append(BuildSoundResourceCatalogEntry(
		state,
		sound.indexPath.c_str(),
		10,
		24));
	manifest.append(BuildGenericResourceCatalogEntry(
		unsupportedSound.indexPath.c_str(),
		11,
		31));

	const ServerDownloadDecision soundDownload = DecideServerDownload(
		manifest.buildDownloadRequest(sound.normalizedPath.c_str(), true, true, true));
	const ServerDownloadDecision genericDownload = DecideServerDownload(
		manifest.buildDownloadRequest(
			unsupportedSound.normalizedPath.c_str(),
			true,
			true,
			true));

	return manifest.size() == 2 &&
		soundDownload.action == ServerDownloadAction::SendFile &&
		soundDownload.resourceIndex == 0 &&
		genericDownload.action == ServerDownloadAction::SendFile &&
		genericDownload.resourceIndex == 1;
}

static bool TestManifestRejectsInvalidDescriptors()
{
	ResourceTransferManifest manifest;
	ResourceDescriptor unnamed = {};
	unnamed.type = ResourceType::Generic;

	ResourceDescriptor unknown = {};
	unknown.name = "sprites/test.spr";
	unknown.type = ResourceType::Unknown;

	ResourceMessageRow row = {};

	return !manifest.append(unnamed) &&
		!manifest.append(unknown) &&
		!manifest.buildResourceMessageRow(0, row) &&
		row.type == ResourceType::Unknown &&
		manifest.empty() &&
		!manifest.data();
}

int main()
{
	if (!TestCatalogEntriesBuildDownloadManifest() ||
		!TestCatalogManifestSizeSummary() ||
		!TestCatalogManifestResourceMessageRow() ||
		!TestReslistTokensCanFeedManifest() ||
		!TestManifestRejectsInvalidDescriptors())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
