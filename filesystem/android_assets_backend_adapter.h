#ifndef XASH_FILESYSTEM_ANDROID_ASSETS_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_ANDROID_ASSETS_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_android_assets_backend_hooks_s
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
} fs_android_assets_backend_hooks_t;

typedef struct fs_android_assets_search_runtime_s
{
	void *context;
	void *(*alloc)(void *context, size_t size, int clear);
	void (*free)(void *context, void *memory);
	stringlist_t *(*listCreate)(void *context);
	void (*listDirectory)(void *context, stringlist_t *list, const char *path);
	void (*listDestroy)(void *context, stringlist_t *list);
	int (*matchPattern)(void *context, const char *text, const char *pattern,
		int caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
} fs_android_assets_search_runtime_t;

typedef struct fs_android_assets_find_runtime_s
{
	void *context;
	void *(*openAsset)(void *context, const char *path, int mode);
	void (*closeAsset)(void *context, void *asset);
} fs_android_assets_find_runtime_t;

typedef struct fs_android_assets_open_runtime_s
{
	void *context;
	void *(*allocFile)(void *context);
	void (*freeFile)(void *context, file_t *file);
	void *(*openAsset)(void *context, const char *path, int mode);
	int (*openFileDescriptor)(void *context, void *asset,
		fs_offset_t *offset, fs_offset_t *length);
	void (*closeAsset)(void *context, void *asset);
	void (*setupFile)(void *context, file_t *file, void *searchPath,
		int handle, fs_offset_t offset, fs_offset_t length);
} fs_android_assets_open_runtime_t;

typedef struct fs_android_assets_load_runtime_s
{
	void *context;
	void *(*openAsset)(void *context, const char *path, int mode);
	fs_offset_t (*length)(void *context, void *asset);
	int (*read)(void *context, void *asset, void *buffer, size_t size);
	void (*closeAsset)(void *context, void *asset);
	void (*allocationFailed)(void *context, size_t size);
} fs_android_assets_load_runtime_t;

void *FS_CreateAndroidAssetsBackendBridge(searchpath_t *search,
	const fs_android_assets_backend_hooks_t *hooks);
void FS_DestroyAndroidAssetsBackendBridge(void *backend);
void FS_AndroidAssetsBackendBridge_PrintInfo(void *backend, char *dst,
	size_t size);
void FS_AndroidAssetsBackendBridge_Close(void *backend);
file_t *FS_AndroidAssetsBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index);
int FS_AndroidAssetsBackendBridge_FileTime(void *backend, const char *path);
int FS_AndroidAssetsBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len);
void FS_AndroidAssetsBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive);
byte *FS_AndroidAssetsBackendBridge_LoadFile(void *backend, const char *path,
	int index, fs_offset_t *fileSize, void *(*alloc)(size_t),
	void (*freeFn)(void *));
int FS_AndroidAssetsBackend_FindAsset(
	const fs_android_assets_find_runtime_t *runtime, const char *path,
	char *fixedName, size_t fixedNameSize);
void FS_AndroidAssetsBackend_SearchAssets(
	const fs_android_assets_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive);
file_t *FS_AndroidAssetsBackend_OpenAsset(
	const fs_android_assets_open_runtime_t *runtime, void *searchPath,
	const char *filename);
byte *FS_AndroidAssetsBackend_LoadAsset(
	const fs_android_assets_load_runtime_t *runtime, const char *path,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *));

#ifdef __cplusplus
}
#endif

#endif
