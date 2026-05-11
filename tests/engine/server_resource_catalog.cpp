#include <cstdlib>
#include <cstring>

#include "engine/server/resources/server_resource_catalog.hpp"

using namespace xash::engine::server;

static bool EntryMatches(
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

static bool TestEmptyEntriesAreSkipped()
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

static bool TestGenericAndEventEntries()
{
	const ResourceCatalogEntry generic =
		BuildGenericResourceCatalogEntry("maps/test.res", 3, 44);
	const ResourceCatalogEntry event =
		BuildEventScriptResourceCatalogEntry("events/test.sc", 9, 55);

	return ResourceCatalogNeedsFileSize(ResourceType::Generic, "maps/test.res") &&
		ResourceCatalogNeedsFileSize(ResourceType::EventScript, "events/test.sc") &&
		EntryMatches(
			generic,
			ResourceType::Generic,
			"maps/test.res",
			3,
			44,
			kResourceCatalogFlagFatalIfMissing) &&
		EntryMatches(
			event,
			ResourceType::EventScript,
			"events/test.sc",
			9,
			55,
			kResourceCatalogFlagFatalIfMissing);
}

static bool TestSoundSentenceMarkerIsAddedOnce()
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
		!EntryMatches(
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

	return ResourceCatalogNeedsFileSize(ResourceType::Sound, "weapons/test.wav") &&
		EntryMatches(regular, ResourceType::Sound, "weapons/test.wav", 14, 777, 0);
}

static bool TestSoundStateResetAllowsNewMarker()
{
	ResourceCatalogState state = {};
	ResetResourceCatalogState(state);
	BuildSoundResourceCatalogEntry(state, "!FIRST", 1, 0);
	ResetResourceCatalogState(state);

	const ResourceCatalogEntry entry =
		BuildSoundResourceCatalogEntry(state, "!SECOND", 2, 0);

	return EntryMatches(
		entry,
		ResourceType::Sound,
		"!",
		2,
		0,
		kResourceCatalogFlagFatalIfMissing);
}

static bool TestModelSizeAndFlags()
{
	const ResourceCatalogEntry external =
		BuildModelResourceCatalogEntry("models/player.mdl", 4, 1000, 5);
	const ResourceCatalogEntry inlineModel =
		BuildModelResourceCatalogEntry("*12", 5, 1000, 7);

	return ResourceCatalogNeedsFileSize(ResourceType::Model, "models/player.mdl") &&
		!ResourceCatalogNeedsFileSize(ResourceType::Model, "*12") &&
		EntryMatches(external, ResourceType::Model, "models/player.mdl", 4, 1000, 5) &&
		EntryMatches(inlineModel, ResourceType::Model, "*12", 5, 0, 7);
}

static bool TestDecalEntry()
{
	const ResourceCatalogEntry decal = BuildDecalResourceCatalogEntry("{shot1", 0);

	return !ResourceCatalogNeedsFileSize(ResourceType::Decal, "{shot1") &&
		EntryMatches(decal, ResourceType::Decal, "{shot1", 0, 0, 0);
}

int main()
{
	if (!TestEmptyEntriesAreSkipped() ||
		!TestGenericAndEventEntries() ||
		!TestSoundSentenceMarkerIsAddedOnce() ||
		!TestSoundStateResetAllowsNewMarker() ||
		!TestModelSizeAndFlags() ||
		!TestDecalEntry())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
