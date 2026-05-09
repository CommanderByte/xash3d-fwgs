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

#ifdef __cplusplus
}
#endif

#endif
