#include "engine/filesystem/mount_flags.hpp"
#include "engine/filesystem/mount_flags_adapter.h"

static_assert(XASH_ENGINE_FS_MOUNT_HD == xash::engine::filesystem::kMountHighDefinitionFlag,
	"high definition mount flag must match the C adapter");
static_assert(XASH_ENGINE_FS_MOUNT_LV == xash::engine::filesystem::kMountLowViolenceFlag,
	"low violence mount flag must match the C adapter");
static_assert(XASH_ENGINE_FS_MOUNT_ADDON == xash::engine::filesystem::kMountAddonFlag,
	"addon mount flag must match the C adapter");
static_assert(XASH_ENGINE_FS_MOUNT_L10N == xash::engine::filesystem::kMountLocalizationFlag,
	"localization mount flag must match the C adapter");

extern "C" uint32_t Xash_BuildFilesystemMountFlags(
	int mountHighDefinition,
	int mountLowViolence,
	int mountAddon,
	int mountLocalization)
{
	xash::engine::filesystem::MountContentSelection selection;
	selection.highDefinition = mountHighDefinition != 0;
	selection.lowViolence = mountLowViolence != 0;
	selection.addon = mountAddon != 0;
	selection.localization = mountLocalization != 0;

	return xash::engine::filesystem::BuildMountFlags(selection);
}
