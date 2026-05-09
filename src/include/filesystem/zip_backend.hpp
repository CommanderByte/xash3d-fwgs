#ifndef XASH_FILESYSTEM_ZIP_BACKEND_HPP
#define XASH_FILESYSTEM_ZIP_BACKEND_HPP

#include "filesystem/search_path_backend.hpp"

namespace xash
{
namespace filesystem
{

struct ZipBackendOps
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

class ZipBackend final : public ISearchPathBackend
{
public:
	explicit ZipBackend(const SearchPathMetadata &metadata);
	ZipBackend(const SearchPathMetadata &metadata, const ZipBackendOps &ops);

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
	ZipBackendOps m_ops;
	bool m_hasOps;
};

enum class ZipLoadStatus
{
	Ok = 0,
	CouldNotOpen = 1,
	BadHeader = 2,
	BadFolders = 3,
	NoFiles = 4,
	Corrupted = 5,
};

enum class ZipCompression
{
	Stored = 0,
	Deflated = 8,
};

struct ZipFileEntry
{
	char name[MAX_SYSPATH];
	fs_offset_t offset;
	fs_offset_t size;
	fs_offset_t compressedSize;
	unsigned short flags;
};

struct ZipArchiveView
{
	const char *source;
	file_t *handle;
	int fileCount;
	ZipFileEntry *files;
};

struct ZipOpenResult
{
	file_t *handle;
	int fileCount;
	ZipFileEntry *files;
};

struct ZipOpenRuntime
{
	void *context;
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*close)(void *context, file_t *file);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*length)(void *context, file_t *file);
	void *(*alloc)(void *context, size_t size, bool clear);
	void (*free)(void *context, void *memory);
};

struct ZipSearchRuntime
{
	void *context;
	bool (*matchPattern)(void *context, const char *text, const char *pattern,
		bool caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
};

struct ZipOpenFileRuntime
{
	void *context;
	file_t *(*openHandle)(void *context, file_t *package,
		fs_offset_t offset, fs_offset_t length);
	bool (*setupDeflated)(void *context, file_t *file,
		fs_offset_t compressedSize, const char *filename);
	void (*close)(void *context, file_t *file);
	void (*unsupportedCompression)(void *context, const char *filename);
};

struct ZipLoadFileRuntime
{
	void *context;
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	void *(*tempAlloc)(void *context, size_t size);
	void (*tempFree)(void *context, void *memory);
	void (*allocationFailed)(void *context, size_t size);
	void (*sizeMismatch)(void *context, const char *filename);
	void (*inflateFailed)(void *context, int code);
	void (*decompressFailed)(void *context, const char *filename, int code);
	bool (*inflateRaw)(void *context, const void *compressed,
		size_t compressedSize, void *output, size_t outputSize,
		const char *filename);
	void (*unsupportedCompression)(void *context, const char *filename);
};

void SortZipEntries(ZipFileEntry *files, int fileCount);
int FindFileInZipArchive(const ZipArchiveView &archive, const char *path,
	char *fixedName, size_t fixedNameSize);
void SearchZipArchive(const ZipArchiveView &archive,
	const ZipSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive);
file_t *OpenZipEntry(const ZipArchiveView &archive,
	const ZipOpenFileRuntime &runtime, int index);
byte *LoadZipEntry(const ZipArchiveView &archive,
	const ZipLoadFileRuntime &runtime, int index, fs_offset_t *fileSize,
	void *(*alloc)(size_t), void (*freeFn)(void *));
ZipLoadStatus OpenZipArchive(const ZipOpenRuntime &runtime,
	const char *filename, ZipOpenResult *result);

}
}

#endif
