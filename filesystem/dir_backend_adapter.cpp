#include <new>

#include "dir_backend_adapter.h"

#include "filesystem/directory_backend.hpp"

namespace
{

using xash::filesystem::DirectoryBackend;
using xash::filesystem::DirectoryBackendOps;
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

}
