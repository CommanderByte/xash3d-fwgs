#ifndef XASH_FILESYSTEM_FACADE_ADAPTER_H
#define XASH_FILESYSTEM_FACADE_ADAPTER_H

#include "filesystem.h"

#ifdef __cplusplus
extern "C" {
#endif

void FS_Facade_ClearSearchPath(void);
void FS_Facade_AddGameDirectory(const char *dir, uint flags);
qboolean FS_Facade_Delete(const char *path);
void FS_Facade_CreatePath(char *path);

int FS_Facade_FileExists(const char *filename, int gamedironly);
qboolean FS_Facade_SysFolderExists(const char *path);
file_t *FS_Facade_Open(const char *filepath, const char *mode,
	qboolean gamedironly);
file_t *FS_Facade_OpenReadFile(const char *filename, const char *mode,
	qboolean gamedironly);
int FS_Facade_Close(file_t *file);
int FS_Facade_Seek(file_t *file, fs_offset_t offset, int whence);
fs_offset_t FS_Facade_Tell(const file_t *file);
fs_offset_t FS_Facade_FileHandleSize(const file_t *file);
fs_offset_t FS_Facade_FileSize(const char *filename, qboolean gamedironly);
int FS_Facade_FileTime(const char *filename, qboolean gamedironly);
qboolean FS_Facade_Eof(const file_t *file);
int FS_Facade_Flush(file_t *file);
fs_offset_t FS_Facade_Read(file_t *file, void *buffer, size_t buffersize);
fs_offset_t FS_Facade_Write(file_t *file, const void *data, size_t datasize);
int FS_Facade_Gets(file_t *file, char *string, size_t bufsize);
int FS_Facade_VPrintf(file_t *file, const char *format, va_list ap);

search_t *FS_Facade_Search(const char *pattern, int caseinsensitive,
	int gamedironly);
void FS_Facade_FreeSearch(search_t *search);
const char *FS_Facade_GetDiskPath(const char *name, qboolean gamedironly);
qboolean FS_Facade_FullPathToRelativePath(char *dst, const char *src,
	size_t size);
qboolean FS_Facade_GetRootDirectory(char *path, size_t size);
qboolean FS_Facade_MountArchiveFullpath(const char *path, int flags);
void FS_Facade_CopyApiTable(fs_api_t *api);

void FS_Facade_ReportMissingSearchState(int handle);
void FS_Facade_ReportMissingSearchStateInFunction(const char *functionName,
	int handle);

#ifdef __cplusplus
}
#endif

#endif
