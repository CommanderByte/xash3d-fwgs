#include "engine/server/server_resource_catalog.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

bool IsSentenceSoundName(const char *name)
{
	return name && name[0] == '!';
}

bool IsInlineModelName(const char *name)
{
	return name && name[0] == '*';
}

ResourceCatalogEntry SkipEntry()
{
	ResourceCatalogEntry entry = {};
	entry.resource.type = ResourceType::Unknown;
	return entry;
}

ResourceCatalogEntry AddEntry(
	ResourceType type,
	const char *name,
	int index,
	int downloadSize,
	unsigned int flags)
{
	ResourceCatalogEntry entry = {};
	entry.shouldAdd = true;
	entry.resource.name = name;
	entry.resource.type = type;
	entry.resource.index = index;
	entry.resource.downloadSize = downloadSize;
	entry.resource.flags = flags;
	return entry;
}

}

void ResetResourceCatalogState(ResourceCatalogState &state)
{
	state.soundSentenceMarkerAdded = false;
}

bool ResourceCatalogNeedsFileSize(ResourceType type, const char *name)
{
	if (StringEmptyOrNull(name))
		return false;

	switch (type)
	{
	case ResourceType::Generic:
	case ResourceType::EventScript:
		return true;
	case ResourceType::Sound:
		return !IsSentenceSoundName(name);
	case ResourceType::Model:
		return !IsInlineModelName(name);
	case ResourceType::Decal:
	case ResourceType::Skin:
	case ResourceType::World:
	case ResourceType::Unknown:
	default:
		return false;
	}
}

ResourceCatalogEntry BuildGenericResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(
		ResourceType::Generic,
		name,
		index,
		probedDownloadSize,
		kResourceCatalogFlagFatalIfMissing);
}

ResourceCatalogEntry BuildSoundResourceCatalogEntry(
	ResourceCatalogState &state,
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	if (IsSentenceSoundName(name))
	{
		if (state.soundSentenceMarkerAdded)
			return SkipEntry();

		state.soundSentenceMarkerAdded = true;
		return AddEntry(
			ResourceType::Sound,
			"!",
			index,
			0,
			kResourceCatalogFlagFatalIfMissing);
	}

	return AddEntry(ResourceType::Sound, name, index, probedDownloadSize, 0);
}

ResourceCatalogEntry BuildModelResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize,
	unsigned int flags)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	const int downloadSize =
		ResourceCatalogNeedsFileSize(ResourceType::Model, name) ? probedDownloadSize : 0;

	return AddEntry(ResourceType::Model, name, index, downloadSize, flags);
}

ResourceCatalogEntry BuildDecalResourceCatalogEntry(const char *name, int index)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(ResourceType::Decal, name, index, 0, 0);
}

ResourceCatalogEntry BuildEventScriptResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize)
{
	if (StringEmptyOrNull(name))
		return SkipEntry();

	return AddEntry(
		ResourceType::EventScript,
		name,
		index,
		probedDownloadSize,
		kResourceCatalogFlagFatalIfMissing);
}

}
}
}
