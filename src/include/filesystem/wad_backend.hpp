#ifndef XASH_FILESYSTEM_WAD_BACKEND_HPP
#define XASH_FILESYSTEM_WAD_BACKEND_HPP

#include "filesystem/search_path_backend.hpp"
#include "wadfile.h"

namespace xash
{
namespace filesystem
{

struct WadBackendOps
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

enum class WadLoadStatus
{
	Ok = 0,
	CouldNotOpen = 1,
	BadHeader = 2,
	BadFolders = 3,
	TooManyFiles = 4,
	NoFiles = 5,
	Corrupted = 6,
};

struct WadArchiveTable
{
	int infotableOffset;
	int lumpCount;
	dlumpinfo_t *lumps;
};

struct WadArchiveView
{
	const char *source;
	int lumpCount;
	dlumpinfo_t *lumps;
	file_t *handle;
	int fileTime;
};

struct WadLoadRuntime
{
	void *context;
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	void *(*alloc)(void *context, poolhandle_t pool, size_t size, bool clear);
	void (*free)(void *context, void *memory);
	void (*duplicateLump)(void *context, const char *wadFile, const char *lumpName);
};

struct WadSearchRuntime
{
	void *context;
	bool (*matchPattern)(void *context, const char *text, const char *pattern,
		bool caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
};

struct WadReadRuntime
{
	void *context;
	fs_offset_t (*tell)(void *context, file_t *file);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	void (*corrupted)(void *context, const char *lumpName);
	void (*allocationFailed)(void *context, size_t size);
	void (*shortRead)(void *context, const char *lumpName);
};

struct WadOpenRuntime
{
	void *context;
	file_t *(*openPacked)(void *context, const char *filename);
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*fileTime)(void *context, const char *filename);
	poolhandle_t (*allocPool)(void *context, const char *name);
	void (*freePool)(void *context, poolhandle_t *pool);
	void (*close)(void *context, file_t *file);
	WadLoadRuntime loadRuntime;
};

struct WadOpenResult
{
	file_t *handle;
	poolhandle_t pool;
	int fileTime;
	WadArchiveTable table;
};

class WadBackend final : public ISearchPathBackend
{
public:
	explicit WadBackend(const SearchPathMetadata &metadata);
	WadBackend(const SearchPathMetadata &metadata, const WadBackendOps &ops);

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
	WadBackendOps m_ops;
	bool m_hasOps;
};

signed char WadTypeFromExtension(const char *path);
const char *WadExtensionFromType(signed char lumpType);
dlumpinfo_t *FindWadLump(dlumpinfo_t *lumps, int lumpCount,
	const char *name, signed char matchType);
dlumpinfo_t *InsertWadLumpSorted(dlumpinfo_t *lumps, int *lumpCount,
	const char *name, const dlumpinfo_t &newLump, bool *duplicateExact);
WadLoadStatus LoadWadLumpTable(const WadLoadRuntime &runtime,
	const char *wadFile, file_t *handle, poolhandle_t pool,
	WadArchiveTable *table);
int FindFileInWadArchive(const WadArchiveView &archive, const char *path,
	char *fixedName, size_t fixedNameSize);
void SearchWadArchive(const WadArchiveView &archive,
	const WadSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive);
byte *ReadWadLump(const WadArchiveView &archive, const WadReadRuntime &runtime,
	int lumpIndex, fs_offset_t *lumpSize, void *(*alloc)(size_t),
	void (*freeFn)(void *));
WadLoadStatus OpenWadArchive(const WadOpenRuntime &runtime,
	const char *filename, bool packed, WadOpenResult *result);

}
}

#endif
