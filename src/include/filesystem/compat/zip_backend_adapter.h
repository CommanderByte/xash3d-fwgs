#ifndef XASH_FILESYSTEM_ZIP_BACKEND_ADAPTER_H
#define XASH_FILESYSTEM_ZIP_BACKEND_ADAPTER_H

#include <stddef.h>

#include "filesystem/compat/private/filesystem_private_types.h"

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

typedef struct fs_zip_file_entry_s
{
	char name[MAX_SYSPATH];
	fs_offset_t offset;
	fs_offset_t size;
	fs_offset_t compressedSize;
	unsigned short flags;
} fs_zip_file_entry_t;

typedef struct fs_zip_archive_view_s
{
	const char *source;
	file_t *handle;
	int fileCount;
	fs_zip_file_entry_t *files;
} fs_zip_archive_view_t;

typedef struct fs_zip_open_result_s
{
	file_t *handle;
	int fileCount;
	fs_zip_file_entry_t *files;
} fs_zip_open_result_t;

typedef struct fs_zip_open_runtime_s
{
	void *context;
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*close)(void *context, file_t *file);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*length)(void *context, file_t *file);
	void *(*alloc)(void *context, size_t size, int clear);
	void (*free)(void *context, void *memory);
} fs_zip_open_runtime_t;

typedef struct fs_zip_search_runtime_s
{
	void *context;
	int (*matchPattern)(void *context, const char *text, const char *pattern,
		int caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
} fs_zip_search_runtime_t;

typedef struct fs_zip_open_file_runtime_s
{
	void *context;
	file_t *(*openHandle)(void *context, file_t *package,
		fs_offset_t offset, fs_offset_t length);
	int (*setupDeflated)(void *context, file_t *file,
		fs_offset_t compressedSize, const char *filename);
	void (*close)(void *context, file_t *file);
	void (*unsupportedCompression)(void *context, const char *filename);
} fs_zip_open_file_runtime_t;

typedef struct fs_zip_load_file_runtime_s
{
	void *context;
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	void *(*tempAlloc)(void *context, size_t size);
	void (*tempFree)(void *context, void *memory);
	void (*allocationFailed)(void *context, size_t size);
	void (*sizeMismatch)(void *context, const char *filename);
	void (*inflateFailed)(void *context, int code);
	void (*decompressFailed)(void *context, const char *filename, int code);
	int (*inflateRaw)(void *context, const void *compressed,
		size_t compressedSize, void *output, size_t outputSize,
		const char *filename);
	void (*unsupportedCompression)(void *context, const char *filename);
} fs_zip_load_file_runtime_t;

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
int FS_ZipBackend_OpenArchive(const fs_zip_open_runtime_t *runtime,
	const char *filename, fs_zip_open_result_t *result);
void FS_ZipBackend_SortEntries(fs_zip_file_entry_t *files, int fileCount);
int FS_ZipBackend_FindFileInArchive(const fs_zip_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize);
void FS_ZipBackend_SearchArchive(const fs_zip_archive_view_t *archive,
	const fs_zip_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive);
file_t *FS_ZipBackend_OpenEntry(const fs_zip_archive_view_t *archive,
	const fs_zip_open_file_runtime_t *runtime, int index);
byte *FS_ZipBackend_LoadEntry(const fs_zip_archive_view_t *archive,
	const fs_zip_load_file_runtime_t *runtime, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *));

#ifdef __cplusplus
}
#endif

#endif
