#include <new>

#include "filesystem/compat/pak_backend_adapter.h"

#include "filesystem/pak_backend.hpp"

namespace
{

using xash::filesystem::PakBackend;
using xash::filesystem::PakBackendOps;
using xash::filesystem::SearchPathBackendType;
using xash::filesystem::SearchPathMetadata;

void HookClose(void *context)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (hooks && hooks->close)
		hooks->close(hooks->context);
}

void HookPrintInfo(void *context, char *dst, size_t size)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (hooks && hooks->printInfo)
		hooks->printInfo(hooks->context, dst, size);
}

file_t *HookOpenFile(void *context, const char *path, const char *mode, int index)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (!hooks || !hooks->openFile)
		return NULL;

	return hooks->openFile(hooks->context, path, mode, index);
}

int HookFileTime(void *context, const char *path)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (!hooks || !hooks->fileTime)
		return -1;

	return hooks->fileTime(hooks->context, path);
}

int HookFindFile(void *context, const char *path, char *fixedName, size_t len)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (!hooks || !hooks->findFile)
		return -1;

	return hooks->findFile(hooks->context, path, fixedName, len);
}

void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const fs_pak_backend_hooks_t *hooks =
		static_cast<const fs_pak_backend_hooks_t *>(context);

	if (hooks && hooks->search)
		hooks->search(hooks->context, list, pattern, caseInsensitive ? 1 : 0);
}

struct PakBackendBridge
{
	PakBackendBridge(const SearchPathMetadata &metadata,
		const fs_pak_backend_hooks_t &legacyHooks)
		: hooks(legacyHooks)
		, backend(metadata, MakeOps())
	{
	}

	PakBackendOps MakeOps()
	{
		PakBackendOps ops = {
			&hooks,
			HookClose,
			HookPrintInfo,
			HookOpenFile,
			HookFileTime,
			HookFindFile,
			HookSearch
		};
		return ops;
	}

	fs_pak_backend_hooks_t hooks;
	PakBackend backend;
};

xash::filesystem::PakArchiveView MakeArchiveView(
	const fs_pak_archive_view_t *archive)
{
	xash::filesystem::PakArchiveView view = {
		archive ? archive->source : NULL,
		archive ? archive->handle : NULL,
		archive ? archive->fileCount : 0,
		reinterpret_cast<xash::filesystem::PakFileEntry *>(
			archive ? archive->files : NULL)
	};
	return view;
}

file_t *OpenSystem(void *context, const char *filename, const char *mode)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (!runtime || !runtime->openSystem)
		return NULL;

	return runtime->openSystem(runtime->context, filename, mode);
}

int CloseFile(void *context, file_t *file)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (!runtime || !runtime->close)
		return -1;

	return runtime->close(runtime->context, file);
}

fs_offset_t ReadFile(void *context, file_t *file, void *buffer, size_t size)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, file, buffer, size);
}

int SeekFile(void *context, file_t *file, fs_offset_t offset, int whence)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (!runtime || !runtime->seek)
		return -1;

	return runtime->seek(runtime->context, file, offset, whence);
}

void *AllocMemory(void *context, size_t size, bool clear)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (!runtime || !runtime->alloc)
		return NULL;

	return runtime->alloc(runtime->context, size, clear ? 1 : 0);
}

void FreeMemory(void *context, void *memory)
{
	const fs_pak_open_runtime_t *runtime =
		static_cast<const fs_pak_open_runtime_t *>(context);

	if (runtime && runtime->free)
		runtime->free(runtime->context, memory);
}

bool SearchMatchPattern(void *context, const char *text, const char *pattern,
	bool caseInsensitive)
{
	const fs_pak_search_runtime_t *runtime =
		static_cast<const fs_pak_search_runtime_t *>(context);

	if (!runtime || !runtime->matchPattern)
		return false;

	return runtime->matchPattern(runtime->context, text, pattern,
		caseInsensitive ? 1 : 0) != 0;
}

int SearchStringCount(void *context, stringlist_t *list)
{
	const fs_pak_search_runtime_t *runtime =
		static_cast<const fs_pak_search_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *SearchStringAt(void *context, stringlist_t *list, int index)
{
	const fs_pak_search_runtime_t *runtime =
		static_cast<const fs_pak_search_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void SearchAppend(void *context, stringlist_t *list, const char *text)
{
	const fs_pak_search_runtime_t *runtime =
		static_cast<const fs_pak_search_runtime_t *>(context);

	if (runtime && runtime->append)
		runtime->append(runtime->context, list, text);
}

file_t *OpenHandle(void *context, file_t *package, int offset, int length)
{
	const fs_pak_open_file_runtime_t *runtime =
		static_cast<const fs_pak_open_file_runtime_t *>(context);

	if (!runtime || !runtime->openHandle)
		return NULL;

	return runtime->openHandle(runtime->context, package, offset, length);
}

PakBackend *BackendFromHandle(void *backend)
{
	PakBackendBridge *bridge = static_cast<PakBackendBridge *>(backend);

	return bridge ? &bridge->backend : NULL;
}

}

extern "C" {

void *FS_CreatePakBackendBridge(searchpath_t *search,
	const fs_pak_backend_hooks_t *hooks)
{
	if (!search || !hooks)
		return NULL;

	SearchPathMetadata metadata = {
		search->filename,
		SearchPathBackendType::Pak,
		search->flags,
		0,
		NULL
	};
	return new (std::nothrow) PakBackendBridge(metadata, *hooks);
}

void FS_DestroyPakBackendBridge(void *backend)
{
	delete static_cast<PakBackendBridge *>(backend);
}

void FS_PakBackendBridge_PrintInfo(void *backend, char *dst, size_t size)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		pakBackend->printInfo(dst, size);
}

void FS_PakBackendBridge_Close(void *backend)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		pakBackend->close();
}

file_t *FS_PakBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		return pakBackend->openFile(path, mode, index);

	return NULL;
}

int FS_PakBackendBridge_FileTime(void *backend, const char *path)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		return pakBackend->fileTime(path);

	return -1;
}

int FS_PakBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		return pakBackend->findFile(path, fixedName, len);

	return -1;
}

void FS_PakBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (PakBackend *pakBackend = BackendFromHandle(backend))
		pakBackend->search(list, pattern, caseInsensitive != 0);
}

int FS_PakBackend_OpenArchive(const fs_pak_open_runtime_t *runtime,
	const char *filename, fs_pak_open_result_t *result)
{
	if (!runtime || !result)
		return static_cast<int>(xash::filesystem::PakLoadStatus::CouldNotOpen);

	xash::filesystem::PakOpenRuntime cppRuntime = {
		const_cast<fs_pak_open_runtime_t *>(runtime),
		OpenSystem,
		CloseFile,
		ReadFile,
		SeekFile,
		AllocMemory,
		FreeMemory
	};
	xash::filesystem::PakOpenResult cppResult;
	const xash::filesystem::PakLoadStatus status =
		xash::filesystem::OpenPakArchive(cppRuntime, filename, &cppResult);

	result->handle = cppResult.handle;
	result->fileCount = cppResult.fileCount;
	result->files = reinterpret_cast<fs_pak_file_entry_t *>(cppResult.files);

	return static_cast<int>(status);
}

void FS_PakBackend_SortEntries(fs_pak_file_entry_t *files, int fileCount)
{
	xash::filesystem::SortPakEntries(
		reinterpret_cast<xash::filesystem::PakFileEntry *>(files), fileCount);
}

int FS_PakBackend_FindFileInArchive(const fs_pak_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	if (!archive)
		return -1;

	return xash::filesystem::FindFileInPakArchive(
		MakeArchiveView(archive), path, fixedName, fixedNameSize);
}

void FS_PakBackend_SearchArchive(const fs_pak_archive_view_t *archive,
	const fs_pak_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (!archive || !runtime)
		return;

	xash::filesystem::PakSearchRuntime cppRuntime = {
		const_cast<fs_pak_search_runtime_t *>(runtime),
		SearchMatchPattern,
		SearchStringCount,
		SearchStringAt,
		SearchAppend
	};

	xash::filesystem::SearchPakArchive(
		MakeArchiveView(archive), cppRuntime, list, pattern,
		caseInsensitive != 0);
}

file_t *FS_PakBackend_OpenEntry(const fs_pak_archive_view_t *archive,
	const fs_pak_open_file_runtime_t *runtime, int index)
{
	if (!archive || !runtime)
		return NULL;

	xash::filesystem::PakOpenFileRuntime cppRuntime = {
		const_cast<fs_pak_open_file_runtime_t *>(runtime),
		OpenHandle
	};

	return xash::filesystem::OpenPakEntry(
		MakeArchiveView(archive), cppRuntime, index);
}

}
