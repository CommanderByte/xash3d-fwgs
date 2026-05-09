#include "filesystem/compat/filesystem_facade_adapter.h"
#include "filesystem/compat/private/filesystem_private_api.h"

extern "C" {

void FS_Facade_ClearSearchPath(void)
{
	FS_ClearSearchPath();
}

void FS_Facade_AddGameDirectory(const char *dir, uint flags)
{
	FS_AddGameDirectory(dir, flags);
}

qboolean FS_Facade_Delete(const char *path)
{
	return FS_Delete(path);
}

void FS_Facade_CreatePath(char *path)
{
	FS_CreatePath(path);
}

int FS_Facade_FileExists(const char *filename, int gamedironly)
{
	return FS_FileExists(filename, gamedironly);
}

qboolean FS_Facade_SysFolderExists(const char *path)
{
	return FS_SysFolderExists(path);
}

file_t *FS_Facade_Open(const char *filepath, const char *mode,
	qboolean gamedironly)
{
	return FS_Open(filepath, mode, gamedironly);
}

file_t *FS_Facade_OpenReadFile(const char *filename, const char *mode,
	qboolean gamedironly)
{
	return FS_OpenReadFile(filename, mode, gamedironly);
}

int FS_Facade_Close(file_t *file)
{
	return FS_Close(file);
}

int FS_Facade_Seek(file_t *file, fs_offset_t offset, int whence)
{
	return FS_Seek(file, offset, whence);
}

fs_offset_t FS_Facade_Tell(const file_t *file)
{
	return FS_Tell(file);
}

fs_offset_t FS_Facade_FileHandleSize(const file_t *file)
{
	return file->real_length;
}

fs_offset_t FS_Facade_FileSize(const char *filename, qboolean gamedironly)
{
	return FS_FileSize(filename, gamedironly);
}

int FS_Facade_FileTime(const char *filename, qboolean gamedironly)
{
	return FS_FileTime(filename, gamedironly);
}

qboolean FS_Facade_Eof(const file_t *file)
{
	return FS_Eof(file);
}

int FS_Facade_Flush(file_t *file)
{
	return FS_Flush(file);
}

fs_offset_t FS_Facade_Read(file_t *file, void *buffer, size_t buffersize)
{
	return FS_Read(file, buffer, buffersize);
}

fs_offset_t FS_Facade_Write(file_t *file, const void *data, size_t datasize)
{
	return FS_Write(file, data, datasize);
}

int FS_Facade_Gets(file_t *file, char *string, size_t bufsize)
{
	return FS_Gets(file, string, bufsize);
}

int FS_Facade_VPrintf(file_t *file, const char *format, va_list ap)
{
	return FS_VPrintf(file, format, ap);
}

search_t *FS_Facade_Search(const char *pattern, int caseinsensitive,
	int gamedironly)
{
	return FS_Search(pattern, caseinsensitive, gamedironly);
}

void FS_Facade_FreeSearch(search_t *search)
{
	Mem_Free(search);
}

const char *FS_Facade_GetDiskPath(const char *name, qboolean gamedironly)
{
	return FS_GetDiskPath(name, gamedironly);
}

qboolean FS_Facade_FullPathToRelativePath(char *dst, const char *src,
	size_t size)
{
	return FS_FullPathToRelativePath(dst, src, size);
}

qboolean FS_Facade_GetRootDirectory(char *path, size_t size)
{
	return FS_GetRootDirectory(path, size);
}

qboolean FS_Facade_MountArchiveFullpath(const char *path, int flags)
{
	return FS_MountArchive_Fullpath(path, flags) != NULL;
}

void FS_Facade_CopyApiTable(fs_api_t *api)
{
	if (api)
		*api = g_api;
}

void FS_Facade_ReportMissingSearchState(int handle)
{
	Con_DPrintf("Can't find search state by handle %d\n", handle);
}

void FS_Facade_ReportMissingSearchStateInFunction(const char *functionName,
	int handle)
{
	Con_DPrintf("%s: Can't find search state by handle %d\n",
		functionName ? functionName : "", handle);
}

}
