#include <new>

#include "zip_backend_adapter.h"

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

}
