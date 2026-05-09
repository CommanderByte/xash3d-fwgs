#include <new>

#include "filesystem/compat/android_assets_backend_adapter.h"

#include "filesystem/android_assets_backend.hpp"

namespace
{

using xash::filesystem::AndroidAssetsBackend;
using xash::filesystem::AndroidAssetsBackendOps;
using xash::filesystem::AndroidAssetsFindRuntime;
using xash::filesystem::AndroidAssetsLoadRuntime;
using xash::filesystem::AndroidAssetsOpenRuntime;
using xash::filesystem::AndroidAssetsSearchRuntime;
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

void *SearchAlloc(void *context, size_t size, bool clear)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (!runtime || !runtime->alloc)
		return NULL;

	return runtime->alloc(runtime->context, size, clear ? 1 : 0);
}

void SearchFree(void *context, void *memory)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (runtime && runtime->free)
		runtime->free(runtime->context, memory);
}

stringlist_t *SearchListCreate(void *context)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (!runtime || !runtime->listCreate)
		return NULL;

	return runtime->listCreate(runtime->context);
}

void SearchListDirectory(void *context, stringlist_t *list, const char *path)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (runtime && runtime->listDirectory)
		runtime->listDirectory(runtime->context, list, path);
}

void SearchListDestroy(void *context, stringlist_t *list)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (runtime && runtime->listDestroy)
		runtime->listDestroy(runtime->context, list);
}

bool SearchMatchPattern(void *context, const char *text, const char *pattern,
	bool caseInsensitive)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	return runtime && runtime->matchPattern &&
		runtime->matchPattern(runtime->context, text, pattern,
			caseInsensitive ? 1 : 0) != 0;
}

int SearchStringCount(void *context, stringlist_t *list)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (!runtime || !runtime->stringCount)
		return 0;

	return runtime->stringCount(runtime->context, list);
}

const char *SearchStringAt(void *context, stringlist_t *list, int index)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (!runtime || !runtime->stringAt)
		return NULL;

	return runtime->stringAt(runtime->context, list, index);
}

void SearchAppend(void *context, stringlist_t *list, const char *text)
{
	const fs_android_assets_search_runtime_t *runtime =
		static_cast<const fs_android_assets_search_runtime_t *>(context);

	if (runtime && runtime->append)
		runtime->append(runtime->context, list, text);
}

void *FindOpenAsset(void *context, const char *path, int mode)
{
	const fs_android_assets_find_runtime_t *runtime =
		static_cast<const fs_android_assets_find_runtime_t *>(context);

	if (!runtime || !runtime->openAsset)
		return NULL;

	return runtime->openAsset(runtime->context, path, mode);
}

void FindCloseAsset(void *context, void *asset)
{
	const fs_android_assets_find_runtime_t *runtime =
		static_cast<const fs_android_assets_find_runtime_t *>(context);

	if (runtime && runtime->closeAsset)
		runtime->closeAsset(runtime->context, asset);
}

void *OpenAllocFile(void *context)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (!runtime || !runtime->allocFile)
		return NULL;

	return runtime->allocFile(runtime->context);
}

void OpenFreeFile(void *context, file_t *file)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (runtime && runtime->freeFile)
		runtime->freeFile(runtime->context, file);
}

void *OpenOpenAsset(void *context, const char *path, int mode)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (!runtime || !runtime->openAsset)
		return NULL;

	return runtime->openAsset(runtime->context, path, mode);
}

int OpenFileDescriptor(void *context, void *asset, fs_offset_t *offset,
	fs_offset_t *length)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (!runtime || !runtime->openFileDescriptor)
		return -1;

	return runtime->openFileDescriptor(runtime->context, asset, offset, length);
}

void OpenCloseAsset(void *context, void *asset)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (runtime && runtime->closeAsset)
		runtime->closeAsset(runtime->context, asset);
}

void OpenSetupFile(void *context, file_t *file, void *searchPath, int handle,
	fs_offset_t offset, fs_offset_t length)
{
	const fs_android_assets_open_runtime_t *runtime =
		static_cast<const fs_android_assets_open_runtime_t *>(context);

	if (runtime && runtime->setupFile)
		runtime->setupFile(runtime->context, file, searchPath, handle, offset,
			length);
}

void *LoadOpenAsset(void *context, const char *path, int mode)
{
	const fs_android_assets_load_runtime_t *runtime =
		static_cast<const fs_android_assets_load_runtime_t *>(context);

	if (!runtime || !runtime->openAsset)
		return NULL;

	return runtime->openAsset(runtime->context, path, mode);
}

fs_offset_t LoadLength(void *context, void *asset)
{
	const fs_android_assets_load_runtime_t *runtime =
		static_cast<const fs_android_assets_load_runtime_t *>(context);

	if (!runtime || !runtime->length)
		return 0;

	return runtime->length(runtime->context, asset);
}

int LoadRead(void *context, void *asset, void *buffer, size_t size)
{
	const fs_android_assets_load_runtime_t *runtime =
		static_cast<const fs_android_assets_load_runtime_t *>(context);

	if (!runtime || !runtime->read)
		return -1;

	return runtime->read(runtime->context, asset, buffer, size);
}

void LoadCloseAsset(void *context, void *asset)
{
	const fs_android_assets_load_runtime_t *runtime =
		static_cast<const fs_android_assets_load_runtime_t *>(context);

	if (runtime && runtime->closeAsset)
		runtime->closeAsset(runtime->context, asset);
}

void LoadAllocationFailed(void *context, size_t size)
{
	const fs_android_assets_load_runtime_t *runtime =
		static_cast<const fs_android_assets_load_runtime_t *>(context);

	if (runtime && runtime->allocationFailed)
		runtime->allocationFailed(runtime->context, size);
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

int FS_AndroidAssetsBackend_FindAsset(
	const fs_android_assets_find_runtime_t *runtime, const char *path,
	char *fixedName, size_t fixedNameSize)
{
	AndroidAssetsFindRuntime findRuntime = {
		const_cast<fs_android_assets_find_runtime_t *>(runtime),
		FindOpenAsset,
		FindCloseAsset
	};
	return xash::filesystem::FindAndroidAsset(findRuntime, path, fixedName,
		fixedNameSize);
}

void FS_AndroidAssetsBackend_SearchAssets(
	const fs_android_assets_search_runtime_t *runtime, stringlist_t *list,
	const char *pattern, int caseInsensitive)
{
	AndroidAssetsSearchRuntime searchRuntime = {
		const_cast<fs_android_assets_search_runtime_t *>(runtime),
		SearchAlloc,
		SearchFree,
		SearchListCreate,
		SearchListDirectory,
		SearchListDestroy,
		SearchMatchPattern,
		SearchStringCount,
		SearchStringAt,
		SearchAppend
	};
	xash::filesystem::SearchAndroidAssets(searchRuntime, list, pattern,
		caseInsensitive != 0);
}

file_t *FS_AndroidAssetsBackend_OpenAsset(
	const fs_android_assets_open_runtime_t *runtime, void *searchPath,
	const char *filename)
{
	AndroidAssetsOpenRuntime openRuntime = {
		const_cast<fs_android_assets_open_runtime_t *>(runtime),
		OpenAllocFile,
		OpenFreeFile,
		OpenOpenAsset,
		OpenFileDescriptor,
		OpenCloseAsset,
		OpenSetupFile
	};
	return xash::filesystem::OpenAndroidAsset(openRuntime, searchPath, filename);
}

byte *FS_AndroidAssetsBackend_LoadAsset(
	const fs_android_assets_load_runtime_t *runtime, const char *path,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	AndroidAssetsLoadRuntime loadRuntime = {
		const_cast<fs_android_assets_load_runtime_t *>(runtime),
		LoadOpenAsset,
		LoadLength,
		LoadRead,
		LoadCloseAsset,
		LoadAllocationFailed
	};
	return xash::filesystem::LoadAndroidAsset(loadRuntime, path, fileSize,
		alloc, freeFn);
}

}
