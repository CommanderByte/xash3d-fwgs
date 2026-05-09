#include <new>

#include "wad_backend_adapter.h"

#include "filesystem/wad_backend.hpp"

namespace
{

using xash::filesystem::SearchPathBackendType;
using xash::filesystem::SearchPathMetadata;
using xash::filesystem::WadBackend;
using xash::filesystem::WadBackendOps;

void HookClose(void *context)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (hooks && hooks->close)
		hooks->close(hooks->context);
}

void HookPrintInfo(void *context, char *dst, size_t size)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (hooks && hooks->printInfo)
		hooks->printInfo(hooks->context, dst, size);
}

file_t *HookOpenFile(void *context, const char *path, const char *mode, int index)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (!hooks || !hooks->openFile)
		return NULL;

	return hooks->openFile(hooks->context, path, mode, index);
}

int HookFileTime(void *context, const char *path)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (!hooks || !hooks->fileTime)
		return -1;

	return hooks->fileTime(hooks->context, path);
}

int HookFindFile(void *context, const char *path, char *fixedName, size_t len)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (!hooks || !hooks->findFile)
		return -1;

	return hooks->findFile(hooks->context, path, fixedName, len);
}

void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (hooks && hooks->search)
		hooks->search(hooks->context, list, pattern, caseInsensitive ? 1 : 0);
}

byte *HookLoadFile(void *context, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	const fs_wad_backend_hooks_t *hooks =
		static_cast<const fs_wad_backend_hooks_t *>(context);

	if (!hooks || !hooks->loadFile)
		return NULL;

	return hooks->loadFile(hooks->context, path, index, fileSize, alloc, freeFn);
}

struct WadBackendBridge
{
	WadBackendBridge(const SearchPathMetadata &metadata,
		const fs_wad_backend_hooks_t &legacyHooks)
		: hooks(legacyHooks)
		, backend(metadata, MakeOps())
	{
	}

	WadBackendOps MakeOps()
	{
		WadBackendOps ops = {
			&hooks,
			HookClose,
			HookPrintInfo,
			HookOpenFile,
			HookFileTime,
			HookFindFile,
			HookSearch,
			HookLoadFile
		};
		return ops;
	}

	fs_wad_backend_hooks_t hooks;
	WadBackend backend;
};

fs_offset_t RuntimeRead(void *context, file_t *file, void *buffer, size_t size)
{
	const fs_wad_load_runtime_t *runtime =
		static_cast<const fs_wad_load_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, file, buffer, size);
}

int RuntimeSeek(void *context, file_t *file, fs_offset_t offset, int whence)
{
	const fs_wad_load_runtime_t *runtime =
		static_cast<const fs_wad_load_runtime_t *>(context);

	if (!runtime || !runtime->seek)
		return -1;

	return runtime->seek(runtime->context, file, offset, whence);
}

void *RuntimeAlloc(void *context, poolhandle_t pool, size_t size, bool clear)
{
	const fs_wad_load_runtime_t *runtime =
		static_cast<const fs_wad_load_runtime_t *>(context);

	if (!runtime || !runtime->alloc)
		return NULL;

	return runtime->alloc(runtime->context, pool, size, clear ? 1 : 0);
}

void RuntimeFree(void *context, void *memory)
{
	const fs_wad_load_runtime_t *runtime =
		static_cast<const fs_wad_load_runtime_t *>(context);

	if (runtime && runtime->free)
		runtime->free(runtime->context, memory);
}

void RuntimeDuplicateLump(void *context, const char *wadFile,
	const char *lumpName)
{
	const fs_wad_load_runtime_t *runtime =
		static_cast<const fs_wad_load_runtime_t *>(context);

	if (runtime && runtime->duplicateLump)
		runtime->duplicateLump(runtime->context, wadFile, lumpName);
}

bool SearchMatchPattern(void *context, const char *text, const char *pattern,
	bool caseInsensitive)
{
	const fs_wad_search_runtime_t *runtime =
		static_cast<const fs_wad_search_runtime_t *>(context);

	if (!runtime || !runtime->matchPattern)
		return false;

	return runtime->matchPattern(runtime->context, text, pattern,
		caseInsensitive ? 1 : 0) != 0;
}

int SearchStringCount(void *context, stringlist_t *list)
{
	const fs_wad_search_runtime_t *runtime =
		static_cast<const fs_wad_search_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *SearchStringAt(void *context, stringlist_t *list, int index)
{
	const fs_wad_search_runtime_t *runtime =
		static_cast<const fs_wad_search_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void SearchAppend(void *context, stringlist_t *list, const char *text)
{
	const fs_wad_search_runtime_t *runtime =
		static_cast<const fs_wad_search_runtime_t *>(context);

	if (runtime && runtime->append)
		runtime->append(runtime->context, list, text);
}

fs_offset_t ReadTell(void *context, file_t *file)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (!runtime || !runtime->tell)
		return -1;

	return runtime->tell(runtime->context, file);
}

int ReadSeek(void *context, file_t *file, fs_offset_t offset, int whence)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (!runtime || !runtime->seek)
		return -1;

	return runtime->seek(runtime->context, file, offset, whence);
}

fs_offset_t ReadRead(void *context, file_t *file, void *buffer, size_t size)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, file, buffer, size);
}

void ReadCorrupted(void *context, const char *lumpName)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (runtime && runtime->corrupted)
		runtime->corrupted(runtime->context, lumpName);
}

void ReadAllocationFailed(void *context, size_t size)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (runtime && runtime->allocationFailed)
		runtime->allocationFailed(runtime->context, size);
}

void ReadShortRead(void *context, const char *lumpName)
{
	const fs_wad_read_runtime_t *runtime =
		static_cast<const fs_wad_read_runtime_t *>(context);

	if (runtime && runtime->shortRead)
		runtime->shortRead(runtime->context, lumpName);
}

file_t *OpenPacked(void *context, const char *filename)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (!runtime || !runtime->openPacked)
		return NULL;

	return runtime->openPacked(runtime->context, filename);
}

file_t *OpenSystem(void *context, const char *filename, const char *mode)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (!runtime || !runtime->openSystem)
		return NULL;

	return runtime->openSystem(runtime->context, filename, mode);
}

int OpenFileTime(void *context, const char *filename)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (!runtime || !runtime->fileTime)
		return 0;

	return runtime->fileTime(runtime->context, filename);
}

poolhandle_t OpenAllocPool(void *context, const char *name)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (!runtime || !runtime->allocPool)
		return 0;

	return runtime->allocPool(runtime->context, name);
}

void OpenFreePool(void *context, poolhandle_t *pool)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (runtime && runtime->freePool)
		runtime->freePool(runtime->context, pool);
}

void OpenClose(void *context, file_t *file)
{
	const fs_wad_open_runtime_t *runtime =
		static_cast<const fs_wad_open_runtime_t *>(context);

	if (runtime && runtime->close)
		runtime->close(runtime->context, file);
}

xash::filesystem::WadArchiveView MakeArchiveView(
	const fs_wad_archive_view_t *archive)
{
	xash::filesystem::WadArchiveView view = {
		archive ? archive->source : NULL,
		archive ? archive->lumpCount : 0,
		archive ? archive->lumps : NULL,
		archive ? archive->handle : NULL,
		archive ? archive->fileTime : 0
	};
	return view;
}

WadBackend *BackendFromHandle(void *backend)
{
	WadBackendBridge *bridge = static_cast<WadBackendBridge *>(backend);

	return bridge ? &bridge->backend : NULL;
}

}

extern "C" {

void *FS_CreateWadBackendBridge(searchpath_t *search,
	const fs_wad_backend_hooks_t *hooks)
{
	if (!search || !hooks)
		return NULL;

	SearchPathMetadata metadata = {
		search->filename,
		SearchPathBackendType::Wad,
		search->flags,
		0,
		NULL
	};
	return new (std::nothrow) WadBackendBridge(metadata, *hooks);
}

void FS_DestroyWadBackendBridge(void *backend)
{
	delete static_cast<WadBackendBridge *>(backend);
}

void FS_WadBackendBridge_PrintInfo(void *backend, char *dst, size_t size)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		wadBackend->printInfo(dst, size);
}

void FS_WadBackendBridge_Close(void *backend)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		wadBackend->close();
}

file_t *FS_WadBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		return wadBackend->openFile(path, mode, index);

	return NULL;
}

int FS_WadBackendBridge_FileTime(void *backend, const char *path)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		return wadBackend->fileTime(path);

	return -1;
}

int FS_WadBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		return wadBackend->findFile(path, fixedName, len);

	return -1;
}

void FS_WadBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		wadBackend->search(list, pattern, caseInsensitive != 0);
}

byte *FS_WadBackendBridge_LoadFile(void *backend, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (WadBackend *wadBackend = BackendFromHandle(backend))
		return wadBackend->loadFile(path, index, fileSize, alloc, freeFn);

	return NULL;
}

signed char FS_WadBackend_TypeFromExt(const char *path)
{
	return xash::filesystem::WadTypeFromExtension(path);
}

const char *FS_WadBackend_ExtFromType(signed char lumpType)
{
	return xash::filesystem::WadExtensionFromType(lumpType);
}

dlumpinfo_t *FS_WadBackend_FindLump(dlumpinfo_t *lumps, int lumpCount,
	const char *name, signed char matchType)
{
	return xash::filesystem::FindWadLump(lumps, lumpCount, name, matchType);
}

dlumpinfo_t *FS_WadBackend_InsertLumpSorted(dlumpinfo_t *lumps, int *lumpCount,
	const char *name, const dlumpinfo_t *newLump, int *duplicateExact)
{
	bool duplicate = false;
	dlumpinfo_t *result;

	if (!newLump)
		return NULL;

	result = xash::filesystem::InsertWadLumpSorted(
		lumps, lumpCount, name, *newLump, &duplicate);

	if (duplicateExact)
		*duplicateExact = duplicate ? 1 : 0;

	return result;
}

int FS_WadBackend_LoadLumpTable(const fs_wad_load_runtime_t *runtime,
	const char *wadFile, file_t *handle, poolhandle_t pool,
	fs_wad_archive_table_t *table)
{
	if (!runtime || !table)
		return static_cast<int>(xash::filesystem::WadLoadStatus::CouldNotOpen);

	xash::filesystem::WadLoadRuntime cppRuntime = {
		const_cast<fs_wad_load_runtime_t *>(runtime),
		RuntimeRead,
		RuntimeSeek,
		RuntimeAlloc,
		RuntimeFree,
		RuntimeDuplicateLump
	};
	xash::filesystem::WadArchiveTable cppTable = {
		0,
		0,
		NULL
	};

	const xash::filesystem::WadLoadStatus status =
		xash::filesystem::LoadWadLumpTable(
			cppRuntime, wadFile, handle, pool, &cppTable);

	table->infotableOffset = cppTable.infotableOffset;
	table->lumpCount = cppTable.lumpCount;
	table->lumps = cppTable.lumps;

	return static_cast<int>(status);
}

int FS_WadBackend_FindFileInArchive(const fs_wad_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	if (!archive)
		return -1;

	return xash::filesystem::FindFileInWadArchive(
		MakeArchiveView(archive), path, fixedName, fixedNameSize);
}

void FS_WadBackend_SearchArchive(const fs_wad_archive_view_t *archive,
	const fs_wad_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (!archive || !runtime)
		return;

	xash::filesystem::WadSearchRuntime cppRuntime = {
		const_cast<fs_wad_search_runtime_t *>(runtime),
		SearchMatchPattern,
		SearchStringCount,
		SearchStringAt,
		SearchAppend
	};

	xash::filesystem::SearchWadArchive(
		MakeArchiveView(archive), cppRuntime, list, pattern,
		caseInsensitive != 0);
}

byte *FS_WadBackend_ReadLump(const fs_wad_archive_view_t *archive,
	const fs_wad_read_runtime_t *runtime, int lumpIndex,
	fs_offset_t *lumpSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (!archive || !runtime)
		return NULL;

	xash::filesystem::WadReadRuntime cppRuntime = {
		const_cast<fs_wad_read_runtime_t *>(runtime),
		ReadTell,
		ReadSeek,
		ReadRead,
		ReadCorrupted,
		ReadAllocationFailed,
		ReadShortRead
	};

	return xash::filesystem::ReadWadLump(
		MakeArchiveView(archive), cppRuntime, lumpIndex, lumpSize,
		alloc, freeFn);
}

int FS_WadBackend_OpenArchive(const fs_wad_open_runtime_t *runtime,
	const char *filename, int packed, fs_wad_open_result_t *result)
{
	if (!runtime || !result)
		return static_cast<int>(xash::filesystem::WadLoadStatus::CouldNotOpen);

	xash::filesystem::WadLoadRuntime loadRuntime = {
		const_cast<fs_wad_load_runtime_t *>(&runtime->loadRuntime),
		RuntimeRead,
		RuntimeSeek,
		RuntimeAlloc,
		RuntimeFree,
		RuntimeDuplicateLump
	};
	xash::filesystem::WadOpenRuntime cppRuntime = {
		const_cast<fs_wad_open_runtime_t *>(runtime),
		OpenPacked,
		OpenSystem,
		OpenFileTime,
		OpenAllocPool,
		OpenFreePool,
		OpenClose,
		loadRuntime
	};
	xash::filesystem::WadOpenResult cppResult;

	const xash::filesystem::WadLoadStatus status =
		xash::filesystem::OpenWadArchive(
			cppRuntime, filename, packed != 0, &cppResult);

	result->handle = cppResult.handle;
	result->pool = cppResult.pool;
	result->fileTime = cppResult.fileTime;
	result->table.infotableOffset = cppResult.table.infotableOffset;
	result->table.lumpCount = cppResult.table.lumpCount;
	result->table.lumps = cppResult.table.lumps;

	return static_cast<int>(status);
}

}
