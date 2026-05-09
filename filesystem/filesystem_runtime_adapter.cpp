#include "filesystem_runtime_adapter.h"

#include "filesystem/filesystem_runtime.hpp"
#include "filesystem/valve_path_resolver.hpp"

namespace
{

xash::filesystem::FilesystemRuntime g_filesystemRuntime;

xash::filesystem::SearchPathListOps MakeSearchPathOps(
	const fs_runtime_searchpath_ops_t *ops);
xash::filesystem::FileHandleMemoryOps MakeFileMemoryOps(
	const fs_runtime_file_memory_ops_t *ops);

searchpath_t *RuntimeNext(void *context, searchpath_t *path)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	if (!ops || !ops->next)
		return NULL;

	return ops->next(ops->context, path);
}

void RuntimeSetNext(void *context, searchpath_t *path, searchpath_t *next)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	if (ops && ops->setNext)
		ops->setNext(ops->context, path, next);
}

bool RuntimeIsStatic(void *context, searchpath_t *path)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	return ops && ops->isStatic && ops->isStatic(ops->context, path) != false;
}

void RuntimeClosePath(void *context, searchpath_t *path)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	if (ops && ops->close)
		ops->close(ops->context, path);
}

void RuntimeFreePath(void *context, searchpath_t *path)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	if (ops && ops->freePath)
		ops->freePath(ops->context, path);
}

void RuntimeMarkGamesNotAdded(void *context)
{
	const fs_runtime_searchpath_ops_t *ops =
		static_cast<const fs_runtime_searchpath_ops_t *>(context);

	if (ops && ops->markGamesNotAdded)
		ops->markGamesNotAdded(ops->context);
}

xash::filesystem::SearchPathListOps MakeSearchPathOps(
	const fs_runtime_searchpath_ops_t *ops)
{
	if (!ops)
	{
		xash::filesystem::SearchPathListOps empty = {};
		return empty;
	}

	xash::filesystem::SearchPathListOps result = {
		const_cast<fs_runtime_searchpath_ops_t *>(ops),
		RuntimeNext,
		RuntimeSetNext,
		RuntimeIsStatic,
		RuntimeClosePath,
		RuntimeFreePath,
		RuntimeMarkGamesNotAdded
	};
	return result;
}

file_t *RuntimeAllocFile(void *context, bool clear)
{
	const fs_runtime_file_memory_ops_t *ops =
		static_cast<const fs_runtime_file_memory_ops_t *>(context);

	if (!ops || !ops->alloc)
		return NULL;

	return ops->alloc(ops->context, clear ? 1 : 0);
}

void RuntimeFreeFile(void *context, file_t *file)
{
	const fs_runtime_file_memory_ops_t *ops =
		static_cast<const fs_runtime_file_memory_ops_t *>(context);

	if (ops && ops->free)
		ops->free(ops->context, file);
}

xash::filesystem::FileHandleMemoryOps MakeFileMemoryOps(
	const fs_runtime_file_memory_ops_t *ops)
{
	if (!ops)
	{
		xash::filesystem::FileHandleMemoryOps empty = {};
		return empty;
	}

	xash::filesystem::FileHandleMemoryOps result = {
		const_cast<fs_runtime_file_memory_ops_t *>(ops),
		RuntimeAllocFile,
		RuntimeFreeFile
	};
	return result;
}

}

extern "C" {

void FS_FilesystemState_Reset(void)
{
	g_filesystemRuntime.reset();
}

void FS_FilesystemState_Configure(const char *rootDir, const char *baseDir,
	const char *gameDir, const char *readOnlyDir, const char *language,
	searchpath_t *searchPaths, searchpath_t *writePath,
	qboolean directPathsEnabled)
{
	xash::filesystem::FilesystemStateConfig config = {
		rootDir,
		baseDir,
		gameDir,
		readOnlyDir,
		language,
		searchPaths,
		writePath,
		directPathsEnabled != false
	};
	g_filesystemRuntime.configure(config);
}

const char *FS_FilesystemState_RootDir(void)
{
	return g_filesystemRuntime.state().rootDir();
}

const char *FS_FilesystemState_BaseDir(void)
{
	return g_filesystemRuntime.state().baseDir();
}

const char *FS_FilesystemState_GameDir(void)
{
	return g_filesystemRuntime.state().gameDir();
}

const char *FS_FilesystemState_ReadOnlyDir(void)
{
	return g_filesystemRuntime.state().readOnlyDir();
}

const char *FS_FilesystemState_Language(void)
{
	return g_filesystemRuntime.state().language();
}

void FS_FilesystemState_SetRootDir(const char *value)
{
	g_filesystemRuntime.state().setRootDir(value);
}

void FS_FilesystemState_SetBaseDir(const char *value)
{
	g_filesystemRuntime.state().setBaseDir(value);
}

void FS_FilesystemState_SetGameDir(const char *value)
{
	g_filesystemRuntime.state().setGameDir(value);
}

void FS_FilesystemState_SetReadOnlyDir(const char *value)
{
	g_filesystemRuntime.state().setReadOnlyDir(value);
}

void FS_FilesystemState_SetLanguage(const char *value)
{
	g_filesystemRuntime.state().setLanguage(value);
}

searchpath_t *FS_FilesystemState_SearchPaths(void)
{
	return g_filesystemRuntime.searchPaths();
}

searchpath_t *FS_FilesystemState_WritePath(void)
{
	return g_filesystemRuntime.writePath();
}

void FS_FilesystemState_SetSearchPaths(searchpath_t *value)
{
	g_filesystemRuntime.setSearchPaths(value);
}

void FS_FilesystemState_SetWritePath(searchpath_t *value)
{
	g_filesystemRuntime.setWritePath(value);
}

qboolean FS_FilesystemState_DirectPathsEnabled(void)
{
	return g_filesystemRuntime.state().directPathsEnabled() ? true : false;
}

void FS_FilesystemState_SetDirectPathsEnabled(qboolean enabled)
{
	g_filesystemRuntime.state().setDirectPathsEnabled(enabled != false);
}

void FS_FilesystemRuntime_PrependSearchPath(searchpath_t *path,
	const fs_runtime_searchpath_ops_t *ops)
{
	g_filesystemRuntime.prependSearchPath(path, MakeSearchPathOps(ops));
}

fs_runtime_searchpath_clear_result_t
FS_FilesystemRuntime_ClearDynamicSearchPaths(
	const fs_runtime_searchpath_ops_t *ops)
{
	xash::filesystem::SearchPathClearResult result =
		g_filesystemRuntime.clearDynamicSearchPaths(MakeSearchPathOps(ops));
	fs_runtime_searchpath_clear_result_t cResult = {
		result.searchPaths,
		result.writePath,
		result.keptCount,
		result.removedCount
	};
	return cResult;
}

qboolean FS_FilesystemRuntime_IsWritePathMounted(
	const fs_runtime_searchpath_ops_t *ops)
{
	return g_filesystemRuntime.isWritePathMounted(MakeSearchPathOps(ops))
		? true : false;
}

file_t *FS_FilesystemRuntime_AllocFile(
	const fs_runtime_file_memory_ops_t *ops, int clear)
{
	return g_filesystemRuntime.allocateFile(MakeFileMemoryOps(ops),
		clear != 0);
}

void FS_FilesystemRuntime_FreeFile(
	const fs_runtime_file_memory_ops_t *ops, file_t *file)
{
	g_filesystemRuntime.freeFile(MakeFileMemoryOps(ops), file);
}

fs_runtime_rescan_plan_t FS_FilesystemRuntime_BeginRescan(
	uint32_t flags, const char *language, uint32_t allowedMountFlags,
	uint32_t localizationFlag)
{
	xash::filesystem::FilesystemRescanPlan plan =
		g_filesystemRuntime.beginRescan(flags, language, allowedMountFlags,
			localizationFlag);
	fs_runtime_rescan_plan_t cPlan = {
		plan.mountFlags,
		plan.localizationEnabled ? true : false,
		plan.language
	};
	return cPlan;
}

qboolean FS_FilesystemRuntime_IsValveGameDirectoryId(const char *id)
{
	return xash::filesystem::ValvePathResolver::isGameDirectoryId(id)
		? true : false;
}

const char *FS_FilesystemRuntime_ResolveValvePathId(char *buffer,
	size_t size, const char *id)
{
	searchpath_t *writePath = g_filesystemRuntime.writePath();
	xash::filesystem::ValvePathContext context = {
		g_filesystemRuntime.state().rootDir(),
		g_filesystemRuntime.state().gameDir(),
		writePath ? writePath->filename : ""
	};

	return xash::filesystem::ValvePathResolver::resolveDirectory(buffer,
		size, id, context);
}

}
