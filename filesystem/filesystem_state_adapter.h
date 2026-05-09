#ifndef XASH_FILESYSTEM_STATE_ADAPTER_H
#define XASH_FILESYSTEM_STATE_ADAPTER_H

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void FS_FilesystemState_Reset(void);
void FS_FilesystemState_Configure(const char *rootDir, const char *baseDir,
	const char *gameDir, const char *readOnlyDir, const char *language,
	searchpath_t *searchPaths, searchpath_t *writePath,
	qboolean directPathsEnabled);

const char *FS_FilesystemState_RootDir(void);
const char *FS_FilesystemState_BaseDir(void);
const char *FS_FilesystemState_GameDir(void);
const char *FS_FilesystemState_ReadOnlyDir(void);
const char *FS_FilesystemState_Language(void);

void FS_FilesystemState_SetRootDir(const char *value);
void FS_FilesystemState_SetBaseDir(const char *value);
void FS_FilesystemState_SetGameDir(const char *value);
void FS_FilesystemState_SetReadOnlyDir(const char *value);
void FS_FilesystemState_SetLanguage(const char *value);

searchpath_t *FS_FilesystemState_SearchPaths(void);
searchpath_t *FS_FilesystemState_WritePath(void);
void FS_FilesystemState_SetSearchPaths(searchpath_t *value);
void FS_FilesystemState_SetWritePath(searchpath_t *value);

qboolean FS_FilesystemState_DirectPathsEnabled(void);
void FS_FilesystemState_SetDirectPathsEnabled(qboolean enabled);

#ifdef __cplusplus
}
#endif

#endif
