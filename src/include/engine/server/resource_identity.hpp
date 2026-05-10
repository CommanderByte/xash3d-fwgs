#ifndef XASH_ENGINE_SERVER_RESOURCE_IDENTITY_HPP
#define XASH_ENGINE_SERVER_RESOURCE_IDENTITY_HPP

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

enum class ResourceType
{
	Sound,
	Skin,
	Model,
	Decal,
	Generic,
	EventScript,
	World,
	Unknown,
};

constexpr int kResourceSummaryBucketCount = 8;
constexpr std::size_t kResourceHashSize = 16;
constexpr std::size_t kCustomMd5ResourceNameLength = 36;

struct ResourceDescriptor
{
	const char *name;
	ResourceType type;
	int index;
	int downloadSize;
	unsigned int flags;
	const std::uint8_t *md5Hash;
};

struct ResourceSizeSummary
{
	int totalSize;
	int sizeByType[kResourceSummaryBucketCount];
};

ResourceSizeSummary EmptyResourceSizeSummary();
void AddResourceToSizeSummary(ResourceSizeSummary &summary, const ResourceDescriptor &resource);

bool IsSafeDownloadName(const char *name);
bool IsCustomMd5ResourceName(const char *name);
bool ParseCustomMd5ResourceName(const char *name, std::uint8_t hash[kResourceHashSize]);
bool FormatCustomMd5ResourceName(
	char *out,
	std::size_t capacity,
	const std::uint8_t hash[kResourceHashSize]);

bool ResourceMatchesDownloadName(const ResourceDescriptor &resource, const char *downloadName);
int FindResourceForDownloadName(
	const ResourceDescriptor *resources,
	std::size_t count,
	const char *downloadName);

const char *ResourceTypeName(ResourceType type);

}
}
}

#endif
