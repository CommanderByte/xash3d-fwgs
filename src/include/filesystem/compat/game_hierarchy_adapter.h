#ifndef XASH_FILESYSTEM_GAME_HIERARCHY_ADAPTER_H
#define XASH_FILESYSTEM_GAME_HIERARCHY_ADAPTER_H

#include "filesystem.h"

#ifdef __cplusplus
extern "C"
{
#endif

qboolean FS_AddGameHierarchyRequests(
	const char *dir,
	uint flags,
	const char *readOnlyDir,
	const char *language);

#ifdef __cplusplus
}
#endif

#endif
