#include "filesystem/compat/searchpath_mount_adapter.h"

#include "filesystem/compat/filesystem_runtime_adapter.h"

#include "filesystem/compat/private/filesystem_private_memory.h"

#include "crtlib.h"

#include <string.h>

namespace
{

searchpath_t *AllocSearchPath(void *, int clear)
{
	return static_cast<searchpath_t *>(_Mem_Alloc(
		fs_mempool,
		sizeof(searchpath_t),
		clear != 0,
		__FILE__,
		__LINE__));
}

void FreeSearchPath(void *, searchpath_t *path)
{
	Mem_Free(path);
}

fs_runtime_searchpath_memory_ops_t SearchPathMemoryOps()
{
	fs_runtime_searchpath_memory_ops_t ops = {};
	ops.alloc = AllocSearchPath;
	ops.free = FreeSearchPath;
	return ops;
}

}

extern "C" {

searchpath_t *FS_SearchPath_Alloc(void)
{
	fs_runtime_searchpath_memory_ops_t ops = SearchPathMemoryOps();
	return FS_FilesystemRuntime_AllocSearchPath(&ops, true);
}

void FS_SearchPath_Free(searchpath_t *path)
{
	fs_runtime_searchpath_memory_ops_t ops = SearchPathMemoryOps();
	FS_FilesystemRuntime_FreeSearchPath(&ops, path);
}

void FS_SearchPath_Init(searchpath_t *path, const char *filename,
	searchpathtype_t type, int flags,
	const fs_searchpath_callbacks_t *callbacks)
{
	if (!path)
		return;

	memset(path, 0, sizeof(*path));

	if (filename)
		Q_strncpy(path->filename, filename, sizeof(path->filename));

	path->type = type;
	path->flags = flags;

	if (!callbacks)
		return;

	path->pfnPrintInfo = callbacks->printInfo;
	path->pfnClose = callbacks->close;
	path->pfnOpenFile = callbacks->openFile;
	path->pfnFileTime = callbacks->fileTime;
	path->pfnFindFile = callbacks->findFile;
	path->pfnSearch = callbacks->search;
	path->pfnLoadFile = callbacks->loadFile;
}

}
