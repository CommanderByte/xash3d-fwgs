#ifndef XASH_FILESYSTEM_ZIP_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_ZIP_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_zip_backend_hooks_s
{
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		int caseInsensitive);
	byte *(*loadFile)(void *context, const char *path, int index,
		fs_offset_t *fileSize, void *(*alloc)(size_t),
		void (*freeFn)(void *));
} fs_zip_backend_hooks_t;

void *FS_CreateZipBackendBridge(searchpath_t *search,
	const fs_zip_backend_hooks_t *hooks);
void FS_DestroyZipBackendBridge(void *backend);
void FS_ZipBackendBridge_PrintInfo(void *backend, char *dst, size_t size);
void FS_ZipBackendBridge_Close(void *backend);
file_t *FS_ZipBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index);
int FS_ZipBackendBridge_FileTime(void *backend, const char *path);
int FS_ZipBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len);
void FS_ZipBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive);
byte *FS_ZipBackendBridge_LoadFile(void *backend, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *));

#ifdef __cplusplus
}
#endif

#endif
