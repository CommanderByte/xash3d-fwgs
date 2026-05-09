#ifndef XASH_FILESYSTEM_WAD_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_WAD_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem_internal.h"
#include "wadfile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_wad_backend_hooks_s
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
} fs_wad_backend_hooks_t;

typedef struct fs_wad_archive_table_s
{
	int infotableOffset;
	int lumpCount;
	dlumpinfo_t *lumps;
} fs_wad_archive_table_t;

typedef struct fs_wad_archive_view_s
{
	const char *source;
	int lumpCount;
	dlumpinfo_t *lumps;
	file_t *handle;
	int fileTime;
} fs_wad_archive_view_t;

typedef struct fs_wad_load_runtime_s
{
	void *context;
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	void *(*alloc)(void *context, poolhandle_t pool, size_t size, int clear);
	void (*free)(void *context, void *memory);
	void (*duplicateLump)(void *context, const char *wadFile, const char *lumpName);
} fs_wad_load_runtime_t;

typedef struct fs_wad_search_runtime_s
{
	void *context;
	int (*matchPattern)(void *context, const char *text, const char *pattern,
		int caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
} fs_wad_search_runtime_t;

typedef struct fs_wad_read_runtime_s
{
	void *context;
	fs_offset_t (*tell)(void *context, file_t *file);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	void (*corrupted)(void *context, const char *lumpName);
	void (*allocationFailed)(void *context, size_t size);
	void (*shortRead)(void *context, const char *lumpName);
} fs_wad_read_runtime_t;

typedef struct fs_wad_open_runtime_s
{
	void *context;
	file_t *(*openPacked)(void *context, const char *filename);
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*fileTime)(void *context, const char *filename);
	poolhandle_t (*allocPool)(void *context, const char *name);
	void (*freePool)(void *context, poolhandle_t *pool);
	void (*close)(void *context, file_t *file);
	fs_wad_load_runtime_t loadRuntime;
} fs_wad_open_runtime_t;

typedef struct fs_wad_open_result_s
{
	file_t *handle;
	poolhandle_t pool;
	int fileTime;
	fs_wad_archive_table_t table;
} fs_wad_open_result_t;

void *FS_CreateWadBackendBridge(searchpath_t *search,
	const fs_wad_backend_hooks_t *hooks);
void FS_DestroyWadBackendBridge(void *backend);
void FS_WadBackendBridge_PrintInfo(void *backend, char *dst, size_t size);
void FS_WadBackendBridge_Close(void *backend);
file_t *FS_WadBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index);
int FS_WadBackendBridge_FileTime(void *backend, const char *path);
int FS_WadBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len);
void FS_WadBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive);
byte *FS_WadBackendBridge_LoadFile(void *backend, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *));

signed char FS_WadBackend_TypeFromExt(const char *path);
const char *FS_WadBackend_ExtFromType(signed char lumpType);
dlumpinfo_t *FS_WadBackend_FindLump(dlumpinfo_t *lumps, int lumpCount,
	const char *name, signed char matchType);
dlumpinfo_t *FS_WadBackend_InsertLumpSorted(dlumpinfo_t *lumps, int *lumpCount,
	const char *name, const dlumpinfo_t *newLump, int *duplicateExact);
int FS_WadBackend_LoadLumpTable(const fs_wad_load_runtime_t *runtime,
	const char *wadFile, file_t *handle, poolhandle_t pool,
	fs_wad_archive_table_t *table);
int FS_WadBackend_FindFileInArchive(const fs_wad_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize);
void FS_WadBackend_SearchArchive(const fs_wad_archive_view_t *archive,
	const fs_wad_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive);
byte *FS_WadBackend_ReadLump(const fs_wad_archive_view_t *archive,
	const fs_wad_read_runtime_t *runtime, int lumpIndex,
	fs_offset_t *lumpSize, void *(*alloc)(size_t), void (*freeFn)(void *));
int FS_WadBackend_OpenArchive(const fs_wad_open_runtime_t *runtime,
	const char *filename, int packed, fs_wad_open_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
