#include "engine/filesystem/mount_flags.hpp"

namespace xash
{
namespace engine
{
namespace filesystem
{

MountContentSelection::MountContentSelection()
	: highDefinition(false)
	, lowViolence(false)
	, addon(false)
	, localization(false)
{
}

std::uint32_t BuildMountFlags(const MountContentSelection &selection)
{
	std::uint32_t flags = 0;

	if (selection.highDefinition)
		flags |= kMountHighDefinitionFlag;

	if (selection.lowViolence)
		flags |= kMountLowViolenceFlag;

	if (selection.addon)
		flags |= kMountAddonFlag;

	if (selection.localization)
		flags |= kMountLocalizationFlag;

	return flags;
}

}
}
}
