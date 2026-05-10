#include <cstdlib>
#include <cstdint>

#include "engine/filesystem/mount_flags.hpp"
#include "engine/filesystem/mount_flags_adapter.h"

using namespace xash::engine::filesystem;

static bool TestDefaultSelectionIsEmpty()
{
	const MountContentSelection selection;

	return BuildMountFlags(selection) == 0;
}

static bool TestIndividualFlags()
{
	MountContentSelection selection;

	selection.highDefinition = true;
	if (BuildMountFlags(selection) != kMountHighDefinitionFlag)
		return false;

	selection = MountContentSelection();
	selection.lowViolence = true;
	if (BuildMountFlags(selection) != kMountLowViolenceFlag)
		return false;

	selection = MountContentSelection();
	selection.addon = true;
	if (BuildMountFlags(selection) != kMountAddonFlag)
		return false;

	selection = MountContentSelection();
	selection.localization = true;
	return BuildMountFlags(selection) == kMountLocalizationFlag;
}

static bool TestCombinedFlags()
{
	MountContentSelection selection;
	selection.highDefinition = true;
	selection.lowViolence = true;
	selection.addon = true;
	selection.localization = true;

	const std::uint32_t expected = kMountHighDefinitionFlag |
		kMountLowViolenceFlag |
		kMountAddonFlag |
		kMountLocalizationFlag;

	return BuildMountFlags(selection) == expected;
}

static bool TestAdapterConstantsMatchCppConstants()
{
	return XASH_ENGINE_FS_MOUNT_HD == kMountHighDefinitionFlag &&
		XASH_ENGINE_FS_MOUNT_LV == kMountLowViolenceFlag &&
		XASH_ENGINE_FS_MOUNT_ADDON == kMountAddonFlag &&
		XASH_ENGINE_FS_MOUNT_L10N == kMountLocalizationFlag;
}

static bool TestAdapterTreatsAnyNonZeroAsEnabled()
{
	const std::uint32_t expected = kMountHighDefinitionFlag |
		kMountLowViolenceFlag |
		kMountAddonFlag |
		kMountLocalizationFlag;

	return Xash_BuildFilesystemMountFlags(1, -1, 2, 3) == expected;
}

int main()
{
	if (!TestDefaultSelectionIsEmpty() ||
		!TestIndividualFlags() ||
		!TestCombinedFlags() ||
		!TestAdapterConstantsMatchCppConstants() ||
		!TestAdapterTreatsAnyNonZeroAsEnabled())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
