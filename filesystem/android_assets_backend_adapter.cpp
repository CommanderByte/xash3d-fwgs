#include <new>

#include "android_assets_backend_adapter.h"

#include "filesystem/android_assets_backend.hpp"

namespace
{

using xash::filesystem::AndroidAssetsBackend;
using xash::filesystem::AndroidAssetsBackendOps;
using xash::filesystem::SearchPathBackendType;
using xash::filesystem::SearchPathMetadata;

void HookClose(void *context)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (hooks && hooks->close)
		hooks->close(hooks->context);
}

void HookPrintInfo(void *context, char *dst, size_t size)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (hooks && hooks->printInfo)
		hooks->printInfo(hooks->context, dst, size);
}

file_t *HookOpenFile(void *context, const char *path, const char *mode,
	int index)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (!hooks || !hooks->openFile)
		return NULL;

	return hooks->openFile(hooks->context, path, mode, index);
}

int HookFileTime(void *context, const char *path)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (!hooks || !hooks->fileTime)
		return -1;

	return hooks->fileTime(hooks->context, path);
}

int HookFindFile(void *context, const char *path, char *fixedName, size_t len)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (!hooks || !hooks->findFile)
		return -1;

	return hooks->findFile(hooks->context, path, fixedName, len);
}

void HookSearch(void *context, stringlist_t *list, const char *pattern,
	bool caseInsensitive)
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (hooks && hooks->search)
		hooks->search(hooks->context, list, pattern, caseInsensitive ? 1 : 0);
}

byte *HookLoadFile(void *context, const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	const fs_android_assets_backend_hooks_t *hooks =
		static_cast<const fs_android_assets_backend_hooks_t *>(context);

	if (!hooks || !hooks->loadFile)
		return NULL;

	return hooks->loadFile(hooks->context, path, index, fileSize, alloc,
		freeFn);
}

struct AndroidAssetsBackendBridge
{
	AndroidAssetsBackendBridge(const SearchPathMetadata &metadata,
		const fs_android_assets_backend_hooks_t &legacyHooks)
		: hooks(legacyHooks)
		, backend(metadata, MakeOps())
	{
	}

	AndroidAssetsBackendOps MakeOps()
	{
		AndroidAssetsBackendOps ops = {
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

	fs_android_assets_backend_hooks_t hooks;
	AndroidAssetsBackend backend;
};

AndroidAssetsBackend *BackendFromHandle(void *backend)
{
	AndroidAssetsBackendBridge *bridge =
		static_cast<AndroidAssetsBackendBridge *>(backend);

	return bridge ? &bridge->backend : NULL;
}

}

extern "C" {

void *FS_CreateAndroidAssetsBackendBridge(searchpath_t *search,
	const fs_android_assets_backend_hooks_t *hooks)
{
	if (!search || !hooks)
		return NULL;

	SearchPathMetadata metadata = {
		search->filename,
		SearchPathBackendType::AndroidAssets,
		search->flags,
		0,
		NULL
	};
	return new (std::nothrow) AndroidAssetsBackendBridge(metadata, *hooks);
}

void FS_DestroyAndroidAssetsBackendBridge(void *backend)
{
	delete static_cast<AndroidAssetsBackendBridge *>(backend);
}

void FS_AndroidAssetsBackendBridge_PrintInfo(void *backend, char *dst,
	size_t size)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		assetsBackend->printInfo(dst, size);
}

void FS_AndroidAssetsBackendBridge_Close(void *backend)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		assetsBackend->close();
}

file_t *FS_AndroidAssetsBackendBridge_OpenFile(void *backend, const char *path,
	const char *mode, int index)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		return assetsBackend->openFile(path, mode, index);

	return NULL;
}

int FS_AndroidAssetsBackendBridge_FileTime(void *backend, const char *path)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		return assetsBackend->fileTime(path);

	return -1;
}

int FS_AndroidAssetsBackendBridge_FindFile(void *backend, const char *path,
	char *fixedName, size_t len)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		return assetsBackend->findFile(path, fixedName, len);

	return -1;
}

void FS_AndroidAssetsBackendBridge_Search(void *backend, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		assetsBackend->search(list, pattern, caseInsensitive != 0);
}

byte *FS_AndroidAssetsBackendBridge_LoadFile(void *backend, const char *path,
	int index, fs_offset_t *fileSize, void *(*alloc)(size_t),
	void (*freeFn)(void *))
{
	if (AndroidAssetsBackend *assetsBackend = BackendFromHandle(backend))
		return assetsBackend->loadFile(path, index, fileSize, alloc, freeFn);

	return NULL;
}

}
