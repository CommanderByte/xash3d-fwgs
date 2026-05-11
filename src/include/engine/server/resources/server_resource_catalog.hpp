#ifndef XASH_ENGINE_SERVER_SERVER_RESOURCE_CATALOG_HPP
#define XASH_ENGINE_SERVER_SERVER_RESOURCE_CATALOG_HPP

#include "engine/server/resources/resource_identity.hpp"

namespace xash
{
namespace engine
{
namespace server
{

constexpr unsigned int kResourceCatalogFlagFatalIfMissing = 1u << 0;

struct ResourceCatalogState
{
	bool soundSentenceMarkerAdded;
};

struct ResourceCatalogEntry
{
	bool shouldAdd;
	ResourceDescriptor resource;
};

void ResetResourceCatalogState(ResourceCatalogState &state);
bool ResourceCatalogNeedsFileSize(ResourceType type, const char *name);

ResourceCatalogEntry BuildGenericResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize);
ResourceCatalogEntry BuildSoundResourceCatalogEntry(
	ResourceCatalogState &state,
	const char *name,
	int index,
	int probedDownloadSize);
ResourceCatalogEntry BuildModelResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize,
	unsigned int flags);
ResourceCatalogEntry BuildDecalResourceCatalogEntry(const char *name, int index);
ResourceCatalogEntry BuildEventScriptResourceCatalogEntry(
	const char *name,
	int index,
	int probedDownloadSize);

}
}
}

#endif
