#ifndef XASH_FILESYSTEM_PAK_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_PAK_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_pak_backend_hooks_s
{
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		int caseInsensitive);
} fs_pak_backend_hooks_t;

typedef struct fs_pak_file_entry_s
{
	char name[56];
	int filepos;
	int filelen;
} fs_pak_file_entry_t;

typedef struct fs_pak_archive_view_s
{
	const char *source;
	file_t *handle;
	int fileCount;
	fs_pak_file_entry_t *files;
} fs_pak_archive_view_t;

typedef struct fs_pak_open_result_s
{
	file_t *handle;
	int fileCount;
	fs_pak_file_entry_t *files;
} fs_pak_open_result_t;

typedef struct fs_pak_open_runtime_s
{
	void *context;
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*close)(void *context, file_t *file);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	void *(*alloc)(void *context, size_t size, int clear);
	void (*free)(void *context, void *memory);
} fs_pak_open_runtime_t;

typedef struct fs_pak_search_runtime_s
{
	void *context;
	int (*matchPattern)(void *context, const char *text, const char *pattern,
		int caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
} fs_pak_search_runtime_t;

typedef struct fs_pak_open_file_runtime_s
{
	void *context;
	file_t *(*openHandle)(void *context, file_t *package,
		int offset, int length);
} fs_pak_open_file_runtime_t;

void *FS_CreatePakBackendBridge(searchpath_t *search,
	const fs_pak_backend_hooks_t *hooks);
void FS_DestroyPakBackendBridge(void *backend);
void FS_PakBackendBridge_PrintInfo(void *backend, char *dst, size_t size);
void FS_PakBackendBridge_Close(void *backend);
file_t *FS_PakBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index);
int FS_PakBackendBridge_FileTime(void *backend, const char *path);
int FS_PakBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len);
void FS_PakBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive);
int FS_PakBackend_OpenArchive(const fs_pak_open_runtime_t *runtime,
	const char *filename, fs_pak_open_result_t *result);
void FS_PakBackend_SortEntries(fs_pak_file_entry_t *files, int fileCount);
int FS_PakBackend_FindFileInArchive(const fs_pak_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize);
void FS_PakBackend_SearchArchive(const fs_pak_archive_view_t *archive,
	const fs_pak_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive);
file_t *FS_PakBackend_OpenEntry(const fs_pak_archive_view_t *archive,
	const fs_pak_open_file_runtime_t *runtime, int index);

#ifdef __cplusplus
}
#endif

#endif
