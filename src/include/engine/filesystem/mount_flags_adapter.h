#ifndef XASH_ENGINE_FILESYSTEM_MOUNT_FLAGS_ADAPTER_H
#define XASH_ENGINE_FILESYSTEM_MOUNT_FLAGS_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	XASH_ENGINE_FS_MOUNT_HD = 1u << 7,
	XASH_ENGINE_FS_MOUNT_LV = 1u << 8,
	XASH_ENGINE_FS_MOUNT_ADDON = 1u << 9,
	XASH_ENGINE_FS_MOUNT_L10N = 1u << 10
};

uint32_t Xash_BuildFilesystemMountFlags(
	int mountHighDefinition,
	int mountLowViolence,
	int mountAddon,
	int mountLocalization);

#ifdef __cplusplus
}
#endif

#endif
