#ifndef XASH_FILESYSTEM_DIR_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_DIR_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_directory_backend_hooks_s
{
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		int caseInsensitive);
} fs_directory_backend_hooks_t;

typedef struct fs_directory_case_runtime_s
{
	void *context;
	void *(*alloc)(void *context, size_t size, int clear);
	void (*free)(void *context, void *memory);
	int (*folderExists)(void *context, const char *path);
	int (*fileExists)(void *context, const char *path);
	int (*fileOrFolderExists)(void *context, const char *path);
	int (*isDirectoryCaseSensitive)(void *context, const char *path);
	stringlist_t *(*listCreate)(void *context);
	void (*listDirectory)(void *context, stringlist_t *list, const char *path,
		int dirsOnly);
	void (*listDestroy)(void *context, stringlist_t *list);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*overflow)(void *context, const char *path, const char *operation);
} fs_directory_case_runtime_t;

typedef struct fs_directory_search_runtime_s
{
	void *context;
	fs_directory_case_runtime_t caseRuntime;
	int (*matchPattern)(void *context, const char *text, const char *pattern,
		int caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
} fs_directory_search_runtime_t;

typedef struct fs_directory_open_runtime_s
{
	void *context;
	fs_directory_case_runtime_t caseRuntime;
	file_t *(*openSystem)(void *context, const char *path, const char *mode);
	void (*setSearchPath)(void *context, file_t *file, void *searchPath);
} fs_directory_open_runtime_t;

void *FS_CreateDirectoryBackendBridge(searchpath_t *search,
	const fs_directory_backend_hooks_t *hooks);
void FS_DestroyDirectoryBackendBridge(void *backend);
void FS_DirectoryBackendBridge_PrintInfo(void *backend, char *dst, size_t size);
void FS_DirectoryBackendBridge_Close(void *backend);
file_t *FS_DirectoryBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index);
int FS_DirectoryBackendBridge_FileTime(void *backend, const char *path);
int FS_DirectoryBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len);
void FS_DirectoryBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive);
void FS_DirectoryBackend_FreeEntries(dir_t *dir,
	const fs_directory_case_runtime_t *runtime);
void FS_DirectoryBackend_PopulateEntries(dir_t *dir, const char *path,
	const fs_directory_case_runtime_t *runtime);
int FS_DirectoryBackend_FindEntry(dir_t *dir, const char *name);
int FS_DirectoryBackend_FixFileCase(dir_t *dir,
	const fs_directory_case_runtime_t *runtime, const char *path, char *dst,
	size_t len, int createPath);
int FS_DirectoryBackend_FindFileInDirectory(dir_t *dir,
	const fs_directory_case_runtime_t *runtime, const char *searchPath,
	const char *path, char *fixedName, size_t fixedNameSize);
void FS_DirectoryBackend_SearchDirectory(dir_t *dir,
	const fs_directory_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive);
file_t *FS_DirectoryBackend_OpenFile(dir_t *dir,
	const fs_directory_open_runtime_t *runtime, void *searchPath,
	const char *rootPath, const char *filename, const char *mode);

#ifdef __cplusplus
}
#endif

#endif
