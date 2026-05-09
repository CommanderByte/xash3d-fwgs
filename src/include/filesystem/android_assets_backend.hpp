#ifndef XASH_FILESYSTEM_ANDROID_ASSETS_BACKEND_HPP
#define XASH_FILESYSTEM_ANDROID_ASSETS_BACKEND_HPP

#include "filesystem/search_path_backend.hpp"

namespace xash
{
namespace filesystem
{

struct AndroidAssetsBackendOps
{
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		bool caseInsensitive);
	byte *(*loadFile)(void *context, const char *path, int index,
		fs_offset_t *fileSize, void *(*alloc)(size_t),
		void (*freeFn)(void *));
};

class AndroidAssetsBackend final : public ISearchPathBackend
{
public:
	explicit AndroidAssetsBackend(const SearchPathMetadata &metadata);
	AndroidAssetsBackend(const SearchPathMetadata &metadata,
		const AndroidAssetsBackendOps &ops);

	const SearchPathMetadata &metadata() const override;
	void printInfo(char *dst, size_t size) const override;
	void close() override;
	file_t *openFile(const char *path, const char *mode, int index) override;
	int fileTime(const char *path) const override;
	int findFile(const char *path, char *fixedName, size_t len) override;
	void search(stringlist_t *list, const char *pattern,
		bool caseInsensitive) override;
	byte *loadFile(const char *path, int index,
		fs_offset_t *fileSize, void *(*alloc)(size_t),
		void (*freeFn)(void *)) override;

private:
	SearchPathMetadata m_metadata;
	AndroidAssetsBackendOps m_ops;
	bool m_hasOps;
};

struct AndroidAssetsSearchRuntime
{
	void *context;
	void *(*alloc)(void *context, size_t size, bool clear);
	void (*free)(void *context, void *memory);
	stringlist_t *(*listCreate)(void *context);
	void (*listDirectory)(void *context, stringlist_t *list, const char *path);
	void (*listDestroy)(void *context, stringlist_t *list);
	bool (*matchPattern)(void *context, const char *text, const char *pattern,
		bool caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
};

struct AndroidAssetsFindRuntime
{
	void *context;
	void *(*openAsset)(void *context, const char *path, int mode);
	void (*closeAsset)(void *context, void *asset);
};

struct AndroidAssetsOpenRuntime
{
	void *context;
	void *(*allocFile)(void *context);
	void (*freeFile)(void *context, file_t *file);
	void *(*openAsset)(void *context, const char *path, int mode);
	int (*openFileDescriptor)(void *context, void *asset,
		fs_offset_t *offset, fs_offset_t *length);
	void (*closeAsset)(void *context, void *asset);
	void (*setupFile)(void *context, file_t *file, void *searchPath,
		int handle, fs_offset_t offset, fs_offset_t length);
};

struct AndroidAssetsLoadRuntime
{
	void *context;
	void *(*openAsset)(void *context, const char *path, int mode);
	fs_offset_t (*length)(void *context, void *asset);
	int (*read)(void *context, void *asset, void *buffer, size_t size);
	void (*closeAsset)(void *context, void *asset);
	void (*allocationFailed)(void *context, size_t size);
};

int FindAndroidAsset(const AndroidAssetsFindRuntime &runtime,
	const char *path, char *fixedName, size_t fixedNameSize);
void SearchAndroidAssets(const AndroidAssetsSearchRuntime &runtime,
	stringlist_t *list, const char *pattern, bool caseInsensitive);
file_t *OpenAndroidAsset(const AndroidAssetsOpenRuntime &runtime,
	void *searchPath, const char *filename);
byte *LoadAndroidAsset(const AndroidAssetsLoadRuntime &runtime,
	const char *path, fs_offset_t *fileSize, void *(*alloc)(size_t),
	void (*freeFn)(void *));

}
}

#endif
