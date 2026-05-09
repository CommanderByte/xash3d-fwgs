#include <new>

#include "filesystem/compat/zip_backend_adapter.h"

#include "filesystem/zip_backend.hpp"

namespace
{

using xash::filesystem::SearchPathBackendType;
using xash::filesystem::SearchPathMetadata;
using xash::filesystem::ZipBackend;
using xash::filesystem::ZipBackendOps;

void HookClose(void *context)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (hooks && hooks->close)
		hooks->close(hooks->context);
}

void HookPrintInfo(void *context, char *dst, size_t size)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (hooks && hooks->printInfo)
		hooks->printInfo(hooks->context, dst, size);
}

file_t *HookOpenFile(void *context, const char *path, const char *mode, int index)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (!hooks || !hooks->openFile)
		return NULL;

	return hooks->openFile(hooks->context, path, mode, index);
}

int HookFileTime(void *context, const char *path)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (!hooks || !hooks->fileTime)
		return -1;

	return hooks->fileTime(hooks->context, path);
}

int HookFindFile(void *context, const char *path, char *fixedName, size_t len)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (!hooks || !hooks->findFile)
		return -1;

	return hooks->findFile(hooks->context, path, fixedName, len);
}

void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (hooks && hooks->search)
		hooks->search(hooks->context, list, pattern, caseInsensitive ? 1 : 0);
}

byte *HookLoadFile(void *context, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	const fs_zip_backend_hooks_t *hooks =
		static_cast<const fs_zip_backend_hooks_t *>(context);

	if (!hooks || !hooks->loadFile)
		return NULL;

	return hooks->loadFile(hooks->context, path, index, fileSize, alloc, freeFn);
}

struct ZipBackendBridge
{
	ZipBackendBridge(const SearchPathMetadata &metadata,
		const fs_zip_backend_hooks_t &legacyHooks)
		: hooks(legacyHooks)
		, backend(metadata, MakeOps())
	{
	}

	ZipBackendOps MakeOps()
	{
		ZipBackendOps ops = {
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

	fs_zip_backend_hooks_t hooks;
	ZipBackend backend;
};

xash::filesystem::ZipArchiveView MakeArchiveView(
	const fs_zip_archive_view_t *archive)
{
	xash::filesystem::ZipArchiveView view = {
		archive ? archive->source : NULL,
		archive ? archive->handle : NULL,
		archive ? archive->fileCount : 0,
		reinterpret_cast<xash::filesystem::ZipFileEntry *>(
			archive ? archive->files : NULL)
	};
	return view;
}

file_t *OpenSystem(void *context, const char *filename, const char *mode)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->openSystem)
		return NULL;

	return runtime->openSystem(runtime->context, filename, mode);
}

int CloseFile(void *context, file_t *file)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->close)
		return -1;

	return runtime->close(runtime->context, file);
}

fs_offset_t ReadFile(void *context, file_t *file, void *buffer, size_t size)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, file, buffer, size);
}

int SeekFile(void *context, file_t *file, fs_offset_t offset, int whence)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->seek)
		return -1;

	return runtime->seek(runtime->context, file, offset, whence);
}

fs_offset_t FileLength(void *context, file_t *file)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->length)
		return -1;

	return runtime->length(runtime->context, file);
}

void *AllocMemory(void *context, size_t size, bool clear)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (!runtime || !runtime->alloc)
		return NULL;

	return runtime->alloc(runtime->context, size, clear ? 1 : 0);
}

void FreeMemory(void *context, void *memory)
{
	const fs_zip_open_runtime_t *runtime =
		static_cast<const fs_zip_open_runtime_t *>(context);

	if (runtime && runtime->free)
		runtime->free(runtime->context, memory);
}

bool SearchMatchPattern(void *context, const char *text, const char *pattern,
	bool caseInsensitive)
{
	const fs_zip_search_runtime_t *runtime =
		static_cast<const fs_zip_search_runtime_t *>(context);

	if (!runtime || !runtime->matchPattern)
		return false;

	return runtime->matchPattern(runtime->context, text, pattern,
		caseInsensitive ? 1 : 0) != 0;
}

int SearchStringCount(void *context, stringlist_t *list)
{
	const fs_zip_search_runtime_t *runtime =
		static_cast<const fs_zip_search_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *SearchStringAt(void *context, stringlist_t *list, int index)
{
	const fs_zip_search_runtime_t *runtime =
		static_cast<const fs_zip_search_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void SearchAppend(void *context, stringlist_t *list, const char *text)
{
	const fs_zip_search_runtime_t *runtime =
		static_cast<const fs_zip_search_runtime_t *>(context);

	if (runtime && runtime->append)
		runtime->append(runtime->context, list, text);
}

file_t *OpenHandle(void *context, file_t *package,
	fs_offset_t offset, fs_offset_t length)
{
	const fs_zip_open_file_runtime_t *runtime =
		static_cast<const fs_zip_open_file_runtime_t *>(context);

	if (!runtime || !runtime->openHandle)
		return NULL;

	return runtime->openHandle(runtime->context, package, offset, length);
}

bool SetupDeflated(void *context, file_t *file,
	fs_offset_t compressedSize, const char *filename)
{
	const fs_zip_open_file_runtime_t *runtime =
		static_cast<const fs_zip_open_file_runtime_t *>(context);

	if (!runtime || !runtime->setupDeflated)
		return false;

	return runtime->setupDeflated(runtime->context, file, compressedSize, filename) != 0;
}

void OpenEntryClose(void *context, file_t *file)
{
	const fs_zip_open_file_runtime_t *runtime =
		static_cast<const fs_zip_open_file_runtime_t *>(context);

	if (runtime && runtime->close)
		runtime->close(runtime->context, file);
}

void OpenEntryUnsupportedCompression(void *context, const char *filename)
{
	const fs_zip_open_file_runtime_t *runtime =
		static_cast<const fs_zip_open_file_runtime_t *>(context);

	if (runtime && runtime->unsupportedCompression)
		runtime->unsupportedCompression(runtime->context, filename);
}

int LoadSeek(void *context, file_t *file, fs_offset_t offset, int whence)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (!runtime || !runtime->seek)
		return -1;

	return runtime->seek(runtime->context, file, offset, whence);
}

fs_offset_t LoadRead(void *context, file_t *file, void *buffer, size_t size)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, file, buffer, size);
}

void *LoadTempAlloc(void *context, size_t size)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (!runtime || !runtime->tempAlloc)
		return NULL;

	return runtime->tempAlloc(runtime->context, size);
}

void LoadTempFree(void *context, void *memory)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->tempFree)
		runtime->tempFree(runtime->context, memory);
}

void LoadAllocationFailed(void *context, size_t size)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->allocationFailed)
		runtime->allocationFailed(runtime->context, size);
}

void LoadSizeMismatch(void *context, const char *filename)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->sizeMismatch)
		runtime->sizeMismatch(runtime->context, filename);
}

void LoadInflateFailed(void *context, int code)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->inflateFailed)
		runtime->inflateFailed(runtime->context, code);
}

void LoadDecompressFailed(void *context, const char *filename, int code)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->decompressFailed)
		runtime->decompressFailed(runtime->context, filename, code);
}

bool LoadInflateRaw(void *context, const void *compressed,
	size_t compressedSize, void *output, size_t outputSize,
	const char *filename)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (!runtime || !runtime->inflateRaw)
		return false;

	return runtime->inflateRaw(runtime->context, compressed, compressedSize,
		output, outputSize, filename) != 0;
}

void LoadUnsupportedCompression(void *context, const char *filename)
{
	const fs_zip_load_file_runtime_t *runtime =
		static_cast<const fs_zip_load_file_runtime_t *>(context);

	if (runtime && runtime->unsupportedCompression)
		runtime->unsupportedCompression(runtime->context, filename);
}

ZipBackend *BackendFromHandle(void *backend)
{
	ZipBackendBridge *bridge = static_cast<ZipBackendBridge *>(backend);

	return bridge ? &bridge->backend : NULL;
}

}

extern "C" {

void *FS_CreateZipBackendBridge(searchpath_t *search,
	const fs_zip_backend_hooks_t *hooks)
{
	if (!search || !hooks)
		return NULL;

	SearchPathMetadata metadata = {
		search->filename,
		SearchPathBackendType::Zip,
		search->flags,
		0,
		NULL
	};
	return new (std::nothrow) ZipBackendBridge(metadata, *hooks);
}

void FS_DestroyZipBackendBridge(void *backend)
{
	delete static_cast<ZipBackendBridge *>(backend);
}

void FS_ZipBackendBridge_PrintInfo(void *backend, char *dst, size_t size)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		zipBackend->printInfo(dst, size);
}

void FS_ZipBackendBridge_Close(void *backend)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		zipBackend->close();
}

file_t *FS_ZipBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		return zipBackend->openFile(path, mode, index);

	return NULL;
}

int FS_ZipBackendBridge_FileTime(void *backend, const char *path)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		return zipBackend->fileTime(path);

	return -1;
}

int FS_ZipBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		return zipBackend->findFile(path, fixedName, len);

	return -1;
}

void FS_ZipBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		zipBackend->search(list, pattern, caseInsensitive != 0);
}

byte *FS_ZipBackendBridge_LoadFile(void *backend, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (ZipBackend *zipBackend = BackendFromHandle(backend))
		return zipBackend->loadFile(path, index, fileSize, alloc, freeFn);

	return NULL;
}

int FS_ZipBackend_OpenArchive(const fs_zip_open_runtime_t *runtime,
	const char *filename, fs_zip_open_result_t *result)
{
	if (!runtime || !result)
		return static_cast<int>(xash::filesystem::ZipLoadStatus::CouldNotOpen);

	xash::filesystem::ZipOpenRuntime cppRuntime = {
		const_cast<fs_zip_open_runtime_t *>(runtime),
		OpenSystem,
		CloseFile,
		ReadFile,
		SeekFile,
		FileLength,
		AllocMemory,
		FreeMemory
	};
	xash::filesystem::ZipOpenResult cppResult;
	const xash::filesystem::ZipLoadStatus status =
		xash::filesystem::OpenZipArchive(cppRuntime, filename, &cppResult);

	result->handle = cppResult.handle;
	result->fileCount = cppResult.fileCount;
	result->files = reinterpret_cast<fs_zip_file_entry_t *>(cppResult.files);

	return static_cast<int>(status);
}

void FS_ZipBackend_SortEntries(fs_zip_file_entry_t *files, int fileCount)
{
	xash::filesystem::SortZipEntries(
		reinterpret_cast<xash::filesystem::ZipFileEntry *>(files), fileCount);
}

int FS_ZipBackend_FindFileInArchive(const fs_zip_archive_view_t *archive,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	if (!archive)
		return -1;

	return xash::filesystem::FindFileInZipArchive(
		MakeArchiveView(archive), path, fixedName, fixedNameSize);
}

void FS_ZipBackend_SearchArchive(const fs_zip_archive_view_t *archive,
	const fs_zip_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (!archive || !runtime)
		return;

	xash::filesystem::ZipSearchRuntime cppRuntime = {
		const_cast<fs_zip_search_runtime_t *>(runtime),
		SearchMatchPattern,
		SearchStringCount,
		SearchStringAt,
		SearchAppend
	};

	xash::filesystem::SearchZipArchive(
		MakeArchiveView(archive), cppRuntime, list, pattern,
		caseInsensitive != 0);
}

file_t *FS_ZipBackend_OpenEntry(const fs_zip_archive_view_t *archive,
	const fs_zip_open_file_runtime_t *runtime, int index)
{
	if (!archive || !runtime)
		return NULL;

	xash::filesystem::ZipOpenFileRuntime cppRuntime = {
		const_cast<fs_zip_open_file_runtime_t *>(runtime),
		OpenHandle,
		SetupDeflated,
		OpenEntryClose,
		OpenEntryUnsupportedCompression
	};

	return xash::filesystem::OpenZipEntry(
		MakeArchiveView(archive), cppRuntime, index);
}

byte *FS_ZipBackend_LoadEntry(const fs_zip_archive_view_t *archive,
	const fs_zip_load_file_runtime_t *runtime, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	if (!archive || !runtime)
		return NULL;

	xash::filesystem::ZipLoadFileRuntime cppRuntime = {
		const_cast<fs_zip_load_file_runtime_t *>(runtime),
		LoadSeek,
		LoadRead,
		LoadTempAlloc,
		LoadTempFree,
		LoadAllocationFailed,
		LoadSizeMismatch,
		LoadInflateFailed,
		LoadDecompressFailed,
		LoadInflateRaw,
		LoadUnsupportedCompression
	};

	return xash::filesystem::LoadZipEntry(
		MakeArchiveView(archive), cppRuntime, index, fileSize, alloc, freeFn);
}

}
