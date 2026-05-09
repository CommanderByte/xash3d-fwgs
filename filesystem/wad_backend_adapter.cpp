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

}
