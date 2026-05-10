#include "engine/server/resource_identity.hpp"

#include "utilities/path.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr const char *kBannedDownloadExtensions[] =
{
	"cfg",
	"lst",
	"ini",
	"log",
	"exe",
	"vbs",
	"com",
	"bat",
	"dll",
	"sys",
	"ps1",
	"so",
	"sh",
	"dylib",
	"apk",
	"ipa",
};

constexpr const char *kCustomMd5Prefix = "!MD5";
constexpr std::size_t kSoundPrefixLength = 6;

bool StringEmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

std::size_t StringLength(const char *value)
{
	return value ? std::strlen(value) : 0;
}

bool StartsWith(const char *value, const char *prefix)
{
	if (!value || !prefix)
		return false;

	return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

char ToLowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value + 'a' - 'A');

	return value;
}

bool IsPrintableAscii(char value)
{
	const unsigned char ch = static_cast<unsigned char>(value);
	return ch >= 32 && ch <= 126;
}

bool IsUppercaseHex(char value)
{
	return (value >= '0' && value <= '9') ||
		(value >= 'A' && value <= 'F');
}

std::uint8_t HexNibble(char value)
{
	if (value >= '0' && value <= '9')
		return static_cast<std::uint8_t>(value - '0');

	if (value >= 'A' && value <= 'F')
		return static_cast<std::uint8_t>(value - 'A' + 10);

	return 0;
}

char HexDigit(std::uint8_t value)
{
	static constexpr char digits[] = "0123456789ABCDEF";
	return digits[value & 0x0F];
}

bool EqualsCaseInsensitiveAscii(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs)
		return false;

	while (*lhs && *rhs)
	{
		if (ToLowerAscii(*lhs) != ToLowerAscii(*rhs))
			return false;

		++lhs;
		++rhs;
	}

	return *lhs == *rhs;
}

bool ContainsBannedPathCharacters(const char *value)
{
	for (const char *cursor = value; cursor && *cursor; ++cursor)
	{
		switch (*cursor)
		{
		case '\\':
		case ':':
		case '~':
			return true;
		default:
			break;
		}
	}

	return false;
}

bool ContainsParentTraversal(const char *value)
{
	if (!value)
		return false;

	return std::strstr(value, "..") != nullptr;
}

bool IsBannedDownloadExtension(const char *extension)
{
	for (const char *banned : kBannedDownloadExtensions)
	{
		if (EqualsCaseInsensitiveAscii(extension, banned))
			return true;
	}

	return false;
}

void CopyLowercase(char *dst, std::size_t capacity, const char *src)
{
	if (!dst || capacity == 0)
		return;

	dst[0] = '\0';

	if (!src)
		return;

	std::size_t i = 0;
	for (; src[i] && i + 1 < capacity; ++i)
		dst[i] = ToLowerAscii(src[i]);

	dst[i] = '\0';
}

int SummaryIndex(ResourceType type, int index)
{
	if (type == ResourceType::Model && index == 1)
		return static_cast<int>(ResourceType::World);

	switch (type)
	{
	case ResourceType::Sound:
	case ResourceType::Skin:
	case ResourceType::Model:
	case ResourceType::Decal:
	case ResourceType::Generic:
	case ResourceType::EventScript:
	case ResourceType::World:
		return static_cast<int>(type);
	case ResourceType::Unknown:
	default:
		return -1;
	}
}

const char *LegacySoundCompareName(const char *downloadName)
{
	if (!downloadName)
		return "";

	const std::size_t length = std::strlen(downloadName);
	if (length < kSoundPrefixLength)
		return "";

	return downloadName + kSoundPrefixLength;
}

}

ResourceSizeSummary EmptyResourceSizeSummary()
{
	ResourceSizeSummary summary = {};
	return summary;
}

void AddResourceToSizeSummary(ResourceSizeSummary &summary, const ResourceDescriptor &resource)
{
	summary.totalSize += resource.downloadSize;

	const int bucket = SummaryIndex(resource.type, resource.index);
	if (bucket >= 0 && bucket < kResourceSummaryBucketCount)
		summary.sizeByType[bucket] += resource.downloadSize;
}

bool IsSafeDownloadName(const char *name)
{
	char lower[4096];
	const char *extension;
	const char *lastDot;
	const std::size_t length = StringLength(name);

	if (StringEmptyOrNull(name))
		return false;

	extension = xash::utilities::FileExtension(name);

	if (StartsWith(name, kCustomMd5Prefix))
	{
		if (extension && extension[0] != '\0')
			return false;

		if (length != kCustomMd5ResourceNameLength)
			return false;

		for (std::size_t i = 4; i < length; ++i)
		{
			if (!IsUppercaseHex(name[i]))
				return false;
		}

		return true;
	}

	for (std::size_t i = 0; i < length; ++i)
	{
		if (!IsPrintableAscii(name[i]))
			return false;
	}

	CopyLowercase(lower, sizeof(lower), name);
	extension = xash::utilities::FileExtension(lower);

	if (ContainsBannedPathCharacters(lower) || ContainsParentTraversal(lower))
		return false;

	if (lower[0] == '/')
		return false;

	lastDot = std::strrchr(lower, '.');
	if (!lastDot)
		return false;

	if (std::strlen(lastDot) != 4)
		return false;

	if (IsBannedDownloadExtension(extension))
		return false;

	return true;
}

bool IsCustomMd5ResourceName(const char *name)
{
	if (!IsSafeDownloadName(name))
		return false;

	return StartsWith(name, kCustomMd5Prefix);
}

bool ParseCustomMd5ResourceName(const char *name, std::uint8_t hash[kResourceHashSize])
{
	if (!hash || !IsCustomMd5ResourceName(name))
		return false;

	for (std::size_t i = 0; i < kResourceHashSize; ++i)
	{
		const char high = name[4 + i * 2];
		const char low = name[5 + i * 2];
		hash[i] = static_cast<std::uint8_t>((HexNibble(high) << 4) | HexNibble(low));
	}

	return true;
}

bool FormatCustomMd5ResourceName(
	char *out,
	std::size_t capacity,
	const std::uint8_t hash[kResourceHashSize])
{
	if (!out || capacity == 0)
		return false;

	out[0] = '\0';

	if (!hash || capacity <= kCustomMd5ResourceNameLength)
		return false;

	std::memcpy(out, kCustomMd5Prefix, 4);

	for (std::size_t i = 0; i < kResourceHashSize; ++i)
	{
		out[4 + i * 2] = HexDigit(static_cast<std::uint8_t>(hash[i] >> 4));
		out[5 + i * 2] = HexDigit(hash[i]);
	}

	out[kCustomMd5ResourceNameLength] = '\0';
	return true;
}

bool ResourceMatchesDownloadName(const ResourceDescriptor &resource, const char *downloadName)
{
	const char *compareName = downloadName ? downloadName : "";

	if (!resource.name)
		return false;

	if (resource.type == ResourceType::Sound)
		compareName = LegacySoundCompareName(downloadName);

	return std::strncmp(resource.name, compareName, 64) == 0;
}

int FindResourceForDownloadName(
	const ResourceDescriptor *resources,
	std::size_t count,
	const char *downloadName)
{
	if (!resources)
		return -1;

	for (std::size_t i = 0; i < count; ++i)
	{
		if (ResourceMatchesDownloadName(resources[i], downloadName))
			return static_cast<int>(i);
	}

	return -1;
}

const char *ResourceTypeName(ResourceType type)
{
	switch (type)
	{
	case ResourceType::Sound:
		return "sound";
	case ResourceType::Skin:
		return "skin";
	case ResourceType::Model:
		return "model";
	case ResourceType::Decal:
		return "decal";
	case ResourceType::Generic:
		return "generic";
	case ResourceType::EventScript:
		return "eventscript";
	case ResourceType::World:
		return "world";
	case ResourceType::Unknown:
	default:
		return "unknown";
	}
}

}
}
}
