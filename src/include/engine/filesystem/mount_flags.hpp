#ifndef XASH_ENGINE_FILESYSTEM_MOUNT_FLAGS_HPP
#define XASH_ENGINE_FILESYSTEM_MOUNT_FLAGS_HPP

#include <cstdint>

namespace xash
{
namespace engine
{
namespace filesystem
{

static const std::uint32_t kMountHighDefinitionFlag = 1u << 7;
static const std::uint32_t kMountLowViolenceFlag = 1u << 8;
static const std::uint32_t kMountAddonFlag = 1u << 9;
static const std::uint32_t kMountLocalizationFlag = 1u << 10;

struct MountContentSelection
{
	MountContentSelection();

	bool highDefinition;
	bool lowViolence;
	bool addon;
	bool localization;
};

std::uint32_t BuildMountFlags(const MountContentSelection &selection);

}
}
}

#endif
