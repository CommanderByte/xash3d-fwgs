#include <cstdlib>
#include <cstring>

#include "engine/server/resource_identity.hpp"

using namespace xash::engine::server;

static bool TestCustomMd5Names()
{
	const char *valid = "!MD50123456789ABCDEF0123456789ABCDEF";
	const char *lowercase = "!MD50123456789abcdef0123456789ABCDEF";
	const char *shortName = "!MD50123456789ABCDE";
	const char *withExtension = "!MD50123456789ABCDEF0123456789AB.exe";
	std::uint8_t hash[kResourceHashSize] = {};
	char formatted[kCustomMd5ResourceNameLength + 1];

	if (!IsCustomMd5ResourceName(valid))
		return false;

	if (IsCustomMd5ResourceName(lowercase) ||
		IsCustomMd5ResourceName(shortName) ||
		IsCustomMd5ResourceName(withExtension))
	{
		return false;
	}

	if (!ParseCustomMd5ResourceName(valid, hash))
		return false;

	if (hash[0] != 0x01 || hash[1] != 0x23 || hash[15] != 0xEF)
		return false;

	if (!FormatCustomMd5ResourceName(formatted, sizeof(formatted), hash))
		return false;

	return std::strcmp(formatted, valid) == 0;
}

static bool TestSafeDownloadNames()
{
	return IsSafeDownloadName("models/player.mdl") &&
		IsSafeDownloadName("sound/player/pl_wade1.wav") &&
		IsSafeDownloadName("a-texture.png") &&
		IsSafeDownloadName("name with spaces.tga") &&
		!IsSafeDownloadName(nullptr) &&
		!IsSafeDownloadName("") &&
		!IsSafeDownloadName("not-a-virus-trust-me.bat") &&
		!IsSafeDownloadName("scripts/config.cfg") &&
		!IsSafeDownloadName("../valve/resource/GameMenu.res") &&
		!IsSafeDownloadName("models\\player.mdl") &&
		!IsSafeDownloadName("/absolute/path.mdl") &&
		!IsSafeDownloadName("extensionless") &&
		!IsSafeDownloadName("archive.longext");
}

static bool TestResourceDownloadMatching()
{
	ResourceDescriptor sound = {};
	sound.name = "player/pl_wade1.wav";
	sound.type = ResourceType::Sound;

	ResourceDescriptor model = {};
	model.name = "models/player.mdl";
	model.type = ResourceType::Model;

	ResourceDescriptor resources[2] = { sound, model };

	return ResourceMatchesDownloadName(sound, "sound/player/pl_wade1.wav") &&
		ResourceMatchesDownloadName(sound, "xxxxxxplayer/pl_wade1.wav") &&
		!ResourceMatchesDownloadName(sound, "player/pl_wade1.wav") &&
		ResourceMatchesDownloadName(model, "models/player.mdl") &&
		!ResourceMatchesDownloadName(model, "sound/models/player.mdl") &&
		FindResourceForDownloadName(resources, 2, "models/player.mdl") == 1 &&
		FindResourceForDownloadName(resources, 2, "missing.wad") == -1;
}

static bool TestResourceSizeSummary()
{
	ResourceSizeSummary summary = EmptyResourceSizeSummary();

	ResourceDescriptor sound = {};
	sound.type = ResourceType::Sound;
	sound.downloadSize = 25;

	ResourceDescriptor world = {};
	world.type = ResourceType::Model;
	world.index = 1;
	world.downloadSize = 100;

	ResourceDescriptor model = {};
	model.type = ResourceType::Model;
	model.index = 2;
	model.downloadSize = 40;

	ResourceDescriptor invalid = {};
	invalid.type = ResourceType::Unknown;
	invalid.downloadSize = 7;

	AddResourceToSizeSummary(summary, sound);
	AddResourceToSizeSummary(summary, world);
	AddResourceToSizeSummary(summary, model);
	AddResourceToSizeSummary(summary, invalid);

	return summary.totalSize == 172 &&
		summary.sizeByType[static_cast<int>(ResourceType::Sound)] == 25 &&
		summary.sizeByType[static_cast<int>(ResourceType::World)] == 100 &&
		summary.sizeByType[static_cast<int>(ResourceType::Model)] == 40 &&
		std::strcmp(ResourceTypeName(ResourceType::EventScript), "eventscript") == 0 &&
		std::strcmp(ResourceTypeName(ResourceType::Unknown), "unknown") == 0;
}

int main()
{
	if (!TestCustomMd5Names() ||
		!TestSafeDownloadNames() ||
		!TestResourceDownloadMatching() ||
		!TestResourceSizeSummary())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
