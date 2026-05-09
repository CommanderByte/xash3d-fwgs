#include <new>

#include "filesystem/compat/dir_backend_adapter.h"

#include "filesystem/directory_backend.hpp"

namespace
{

using xash::filesystem::DirectoryBackend;
using xash::filesystem::DirectoryBackendOps;
using xash::filesystem::DirectoryCaseRuntime;
using xash::filesystem::DirectoryEntry;
using xash::filesystem::DirectoryOpenRuntime;
using xash::filesystem::DirectorySearchRuntime;
using xash::filesystem::SearchPathBackendType;
using xash::filesystem::SearchPathMetadata;

void HookClose(void *context)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (hooks && hooks->close)
		hooks->close(hooks->context);
}

void HookPrintInfo(void *context, char *dst, size_t size)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (hooks && hooks->printInfo)
		hooks->printInfo(hooks->context, dst, size);
}

file_t *HookOpenFile(void *context, const char *path, const char *mode, int index)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (!hooks || !hooks->openFile)
		return NULL;

	return hooks->openFile(hooks->context, path, mode, index);
}

int HookFileTime(void *context, const char *path)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (!hooks || !hooks->fileTime)
		return -1;

	return hooks->fileTime(hooks->context, path);
}

int HookFindFile(void *context, const char *path, char *fixedName, size_t len)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (!hooks || !hooks->findFile)
		return -1;

	return hooks->findFile(hooks->context, path, fixedName, len);
}

void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const fs_directory_backend_hooks_t *hooks =
		static_cast<const fs_directory_backend_hooks_t *>(context);

	if (hooks && hooks->search)
		hooks->search(hooks->context, list, pattern, caseInsensitive ? 1 : 0);
}

SearchPathBackendType BackendTypeFromSearchPath(const searchpath_t *search)
{
	if (!search)
		return SearchPathBackendType::Unknown;

	if (search->type == SEARCHPATH_PK3DIR)
		return SearchPathBackendType::Pk3Directory;

	return SearchPathBackendType::Directory;
}

DirectoryEntry *EntryFromHandle(dir_t *dir)
{
	return reinterpret_cast<DirectoryEntry *>(dir);
}

void *CaseAlloc(void *context, size_t size, bool clear)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (!runtime || !runtime->alloc)
		return NULL;

	return runtime->alloc(runtime->context, size, clear ? 1 : 0);
}

void CaseFree(void *context, void *memory)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (runtime && runtime->free)
		runtime->free(runtime->context, memory);
}

bool CaseFolderExists(void *context, const char *path)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	return runtime && runtime->folderExists &&
		runtime->folderExists(runtime->context, path) != 0;
}

bool CaseFileExists(void *context, const char *path)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	return runtime && runtime->fileExists &&
		runtime->fileExists(runtime->context, path) != 0;
}

bool CaseFileOrFolderExists(void *context, const char *path)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	return runtime && runtime->fileOrFolderExists &&
		runtime->fileOrFolderExists(runtime->context, path) != 0;
}

bool CaseIsDirectoryCaseSensitive(void *context, const char *path)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	return !runtime || !runtime->isDirectoryCaseSensitive ||
		runtime->isDirectoryCaseSensitive(runtime->context, path) != 0;
}

stringlist_t *CaseListCreate(void *context)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (!runtime || !runtime->listCreate)
		return NULL;

	return runtime->listCreate(runtime->context);
}

void CaseListDirectory(void *context, stringlist_t *list, const char *path,
	bool dirsOnly)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (runtime && runtime->listDirectory)
		runtime->listDirectory(runtime->context, list, path, dirsOnly ? 1 : 0);
}

void CaseListDestroy(void *context, stringlist_t *list)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (runtime && runtime->listDestroy)
		runtime->listDestroy(runtime->context, list);
}

int CaseStringCount(void *context, stringlist_t *list)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *CaseStringAt(void *context, stringlist_t *list, int index)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void CaseOverflow(void *context, const char *path, const char *operation)
{
	const fs_directory_case_runtime_t *runtime =
		static_cast<const fs_directory_case_runtime_t *>(context);

	if (runtime && runtime->overflow)
		runtime->overflow(runtime->context, path, operation);
}

DirectoryCaseRuntime MakeCaseRuntime(
	const fs_directory_case_runtime_t *runtime)
{
	DirectoryCaseRuntime result = {
		const_cast<fs_directory_case_runtime_t *>(runtime),
		CaseAlloc,
		CaseFree,
		CaseFolderExists,
		CaseFileExists,
		CaseFileOrFolderExists,
		CaseIsDirectoryCaseSensitive,
		CaseListCreate,
		CaseListDirectory,
		CaseListDestroy,
		CaseStringCount,
		CaseStringAt,
		CaseOverflow
	};
	return result;
}

bool SearchMatchPattern(void *context, const char *text, const char *pattern,
	bool caseInsensitive)
{
	const fs_directory_search_runtime_t *runtime =
		static_cast<const fs_directory_search_runtime_t *>(context);

	return runtime && runtime->matchPattern &&
		runtime->matchPattern(runtime->context, text, pattern,
			caseInsensitive ? 1 : 0) != 0;
}

int SearchStringCount(void *context, stringlist_t *list)
{
	const fs_directory_search_runtime_t *runtime =
		static_cast<const fs_directory_search_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *SearchStringAt(void *context, stringlist_t *list, int index)
{
	const fs_directory_search_runtime_t *runtime =
		static_cast<const fs_directory_search_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void SearchAppend(void *context, stringlist_t *list, const char *text)
{
	const fs_directory_search_runtime_t *runtime =
		static_cast<const fs_directory_search_runtime_t *>(context);

	if (runtime && runtime->append)
		runtime->append(runtime->context, list, text);
}

file_t *OpenSystem(void *context, const char *path, const char *mode)
{
	const fs_directory_open_runtime_t *runtime =
		static_cast<const fs_directory_open_runtime_t *>(context);

	if (!runtime || !runtime->openSystem)
		return NULL;

	return runtime->openSystem(runtime->context, path, mode);
}

void SetSearchPath(void *context, file_t *file, void *searchPath)
{
	const fs_directory_open_runtime_t *runtime =
		static_cast<const fs_directory_open_runtime_t *>(context);

	if (runtime && runtime->setSearchPath)
		runtime->setSearchPath(runtime->context, file, searchPath);
}

struct DirectoryBackendBridge
{
	DirectoryBackendBridge(const SearchPathMetadata &metadata,
		const fs_directory_backend_hooks_t &legacyHooks)
		: hooks(legacyHooks)
		, backend(metadata, MakeOps())
	{
	}

	DirectoryBackendOps MakeOps()
	{
		DirectoryBackendOps ops = {
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

	fs_directory_backend_hooks_t hooks;
	DirectoryBackend backend;
};

DirectoryBackend *BackendFromHandle(void *backend)
{
	DirectoryBackendBridge *bridge = static_cast<DirectoryBackendBridge *>(backend);

	return bridge ? &bridge->backend : NULL;
}

}

extern "C" {

void *FS_CreateDirectoryBackendBridge(searchpath_t *search,
	const fs_directory_backend_hooks_t *hooks)
{
	if (!search || !hooks)
		return NULL;

	SearchPathMetadata metadata = {
		search->filename,
		BackendTypeFromSearchPath(search),
		search->flags,
		0,
		NULL
	};
	return new (std::nothrow) DirectoryBackendBridge(metadata, *hooks);
}

void FS_DestroyDirectoryBackendBridge(void *backend)
{
	delete static_cast<DirectoryBackendBridge *>(backend);
}

void FS_DirectoryBackendBridge_PrintInfo(void *backend, char *dst, size_t size)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		directoryBackend->printInfo(dst, size);
}

void FS_DirectoryBackendBridge_Close(void *backend)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		directoryBackend->close();
}

file_t *FS_DirectoryBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		return directoryBackend->openFile(path, mode, index);

	return NULL;
}

int FS_DirectoryBackendBridge_FileTime(void *backend, const char *path)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		return directoryBackend->fileTime(path);

	return -1;
}

int FS_DirectoryBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		return directoryBackend->findFile(path, fixedName, len);

	return -1;
}

void FS_DirectoryBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (DirectoryBackend *directoryBackend = BackendFromHandle(backend))
		directoryBackend->search(list, pattern, caseInsensitive != 0);
}

void FS_DirectoryBackend_FreeEntries(dir_t *dir,
	const fs_directory_case_runtime_t *runtime)
{
	xash::filesystem::FreeDirectoryEntries(EntryFromHandle(dir),
		MakeCaseRuntime(runtime));
}

void FS_DirectoryBackend_PopulateEntries(dir_t *dir, const char *path,
	const fs_directory_case_runtime_t *runtime)
{
	xash::filesystem::PopulateDirectoryEntries(EntryFromHandle(dir), path,
		MakeCaseRuntime(runtime));
}

int FS_DirectoryBackend_FindEntry(dir_t *dir, const char *name)
{
	return xash::filesystem::FindDirectoryEntry(EntryFromHandle(dir), name);
}

int FS_DirectoryBackend_FixFileCase(dir_t *dir,
	const fs_directory_case_runtime_t *runtime, const char *path, char *dst,
	size_t len, int createPath)
{
	return xash::filesystem::FixDirectoryFileCase(EntryFromHandle(dir),
		MakeCaseRuntime(runtime), path, dst, len, createPath != 0) ? 1 : 0;
}

int FS_DirectoryBackend_FindFileInDirectory(dir_t *dir,
	const fs_directory_case_runtime_t *runtime, const char *searchPath,
	const char *path, char *fixedName, size_t fixedNameSize)
{
	return xash::filesystem::FindFileInDirectory(EntryFromHandle(dir),
		MakeCaseRuntime(runtime), searchPath, path, fixedName, fixedNameSize);
}

void FS_DirectoryBackend_SearchDirectory(dir_t *dir,
	const fs_directory_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (!runtime)
		return;

	DirectorySearchRuntime searchRuntime = {
		const_cast<fs_directory_search_runtime_t *>(runtime),
		MakeCaseRuntime(&runtime->caseRuntime),
		SearchMatchPattern,
		SearchStringCount,
		SearchStringAt,
		SearchAppend
	};
	xash::filesystem::SearchDirectory(EntryFromHandle(dir), searchRuntime, list,
		pattern, caseInsensitive != 0);
}

file_t *FS_DirectoryBackend_OpenFile(dir_t *dir,
	const fs_directory_open_runtime_t *runtime, void *searchPath,
	const char *rootPath, const char *filename, const char *mode)
{
	if (!runtime)
		return NULL;

	DirectoryOpenRuntime openRuntime = {
		const_cast<fs_directory_open_runtime_t *>(runtime),
		MakeCaseRuntime(&runtime->caseRuntime),
		OpenSystem,
		SetSearchPath
	};
	return xash::filesystem::OpenDirectoryFile(EntryFromHandle(dir),
		openRuntime, searchPath, rootPath, filename, mode);
}

}
