#ifndef XASH_FILESYSTEM_RUNTIME_ADAPTER_H
#define XASH_FILESYSTEM_RUNTIME_ADAPTER_H

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_runtime_searchpath_ops_s
{
	void *context;
	searchpath_t *(*next)(void *context, searchpath_t *path);
	void (*setNext)(void *context, searchpath_t *path, searchpath_t *next);
	qboolean (*isStatic)(void *context, searchpath_t *path);
	void (*close)(void *context, searchpath_t *path);
	void (*freePath)(void *context, searchpath_t *path);
	void (*markGamesNotAdded)(void *context);
} fs_runtime_searchpath_ops_t;

typedef struct fs_runtime_searchpath_clear_result_s
{
	searchpath_t *searchPaths;
	searchpath_t *writePath;
	int keptCount;
	int removedCount;
} fs_runtime_searchpath_clear_result_t;

typedef struct fs_runtime_file_memory_ops_s
{
	void *context;
	file_t *(*alloc)(void *context, int clear);
	void (*free)(void *context, file_t *file);
} fs_runtime_file_memory_ops_t;

typedef struct fs_runtime_searchpath_memory_ops_s
{
	void *context;
	searchpath_t *(*alloc)(void *context, int clear);
	void (*free)(void *context, searchpath_t *path);
} fs_runtime_searchpath_memory_ops_t;

typedef struct fs_runtime_rescan_plan_s
{
	uint32_t mountFlags;
	qboolean localizationEnabled;
	const char *language;
} fs_runtime_rescan_plan_t;

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

void FS_FilesystemRuntime_PrependSearchPath(searchpath_t *path,
	const fs_runtime_searchpath_ops_t *ops);
fs_runtime_searchpath_clear_result_t
FS_FilesystemRuntime_ClearDynamicSearchPaths(
	const fs_runtime_searchpath_ops_t *ops);
qboolean FS_FilesystemRuntime_IsWritePathMounted(
	const fs_runtime_searchpath_ops_t *ops);

file_t *FS_FilesystemRuntime_AllocFile(
	const fs_runtime_file_memory_ops_t *ops, int clear);
void FS_FilesystemRuntime_FreeFile(
	const fs_runtime_file_memory_ops_t *ops, file_t *file);
searchpath_t *FS_FilesystemRuntime_AllocSearchPath(
	const fs_runtime_searchpath_memory_ops_t *ops, int clear);
void FS_FilesystemRuntime_FreeSearchPath(
	const fs_runtime_searchpath_memory_ops_t *ops, searchpath_t *path);
fs_runtime_rescan_plan_t FS_FilesystemRuntime_BeginRescan(
	uint32_t flags, const char *language, uint32_t allowedMountFlags,
	uint32_t localizationFlag);
qboolean FS_FilesystemRuntime_IsValveGameDirectoryId(const char *id);
const char *FS_FilesystemRuntime_ResolveValvePathId(char *buffer,
	size_t size, const char *id);

#ifdef __cplusplus
}
#endif

#endif
