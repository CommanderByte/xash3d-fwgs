#ifndef XASH_FILESYSTEM_ARCHIVE_REGISTRY_HPP
#define XASH_FILESYSTEM_ARCHIVE_REGISTRY_HPP

#include <stddef.h>

#include "utilities/registry.hpp"

namespace xash
{
namespace filesystem
{

enum class ArchiveBackendType
{
	Pak,
	Wad,
	Zip,
	Pk3Directory,
	Unknown,
};

struct ArchiveFormatDescriptor
{
	const char *extension;
	ArchiveBackendType backendType;
	bool realArchive;
	bool autoMountContainedWads;
	int scanPriority;
	const char *debugName;
	const char *factoryName;
};

static const size_t ArchiveRegistryCapacity = 16;

typedef xash::utilities::StaticRegistry<
	const char *,
	ArchiveFormatDescriptor,
	ArchiveRegistryCapacity,
	xash::utilities::CaseInsensitiveCStringKeyEqual> ArchiveRegistry;

const char *ArchiveBackendTypeName(ArchiveBackendType type);
const ArchiveFormatDescriptor *DefaultArchiveFormatDescriptors(size_t &count);
xash::utilities::RegistryStatus RegisterDefaultArchiveFormats(
	ArchiveRegistry &registry);

}
}

#endif
