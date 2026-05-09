#include <new>

#include "pak_backend_adapter.h"

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

}
