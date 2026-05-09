#ifndef XASH_FILESYSTEM_PAK_BACKEND_HPP
#define XASH_FILESYSTEM_PAK_BACKEND_HPP

#include "filesystem/search_path_backend.hpp"

namespace xash
{
namespace filesystem
{

struct PakBackendOps
{
	void *context;
	void (*close)(void *context);
	void (*printInfo)(void *context, char *dst, size_t size);
	file_t *(*openFile)(void *context, const char *path, const char *mode, int index);
	int (*fileTime)(void *context, const char *path);
	int (*findFile)(void *context, const char *path, char *fixedName, size_t len);
	void (*search)(void *context, stringlist_t *list, const char *pattern,
		bool caseInsensitive);
};

class PakBackend final : public ISearchPathBackend
{
public:
	explicit PakBackend(const SearchPathMetadata &metadata);
	PakBackend(const SearchPathMetadata &metadata, const PakBackendOps &ops);

	const SearchPathMetadata &metadata() const override;
	void printInfo(char *dst, size_t size) const override;
	void close() override;
	file_t *openFile(const char *path, const char *mode, int index) override;
	int fileTime(const char *path) const override;
	int findFile(const char *path, char *fixedName, size_t len) override;
	void search(stringlist_t *list, const char *pattern,
		bool caseInsensitive) override;

private:
	SearchPathMetadata m_metadata;
	PakBackendOps m_ops;
	bool m_hasOps;
};

struct PakFileEntry
{
	char name[56];
	int filepos;
	int filelen;
};

enum class PakLoadStatus
{
	Ok = 0,
	CouldNotOpen = 1,
	BadHeader = 2,
	BadFolders = 3,
	TooManyFiles = 4,
	NoFiles = 5,
	Corrupted = 6,
};

struct PakArchiveView
{
	const char *source;
	file_t *handle;
	int fileCount;
	PakFileEntry *files;
};

struct PakOpenResult
{
	file_t *handle;
	int fileCount;
	PakFileEntry *files;
};

struct PakOpenRuntime
{
	void *context;
	file_t *(*openSystem)(void *context, const char *filename, const char *mode);
	int (*close)(void *context, file_t *file);
	fs_offset_t (*read)(void *context, file_t *file, void *buffer, size_t size);
	int (*seek)(void *context, file_t *file, fs_offset_t offset, int whence);
	void *(*alloc)(void *context, size_t size, bool clear);
	void (*free)(void *context, void *memory);
};

struct PakSearchRuntime
{
	void *context;
	bool (*matchPattern)(void *context, const char *text, const char *pattern,
		bool caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
};

struct PakOpenFileRuntime
{
	void *context;
	file_t *(*openHandle)(void *context, file_t *package,
		int offset, int length);
};

void SortPakEntries(PakFileEntry *files, int fileCount);
int FindFileInPakArchive(const PakArchiveView &archive, const char *path,
	char *fixedName, size_t fixedNameSize);
void SearchPakArchive(const PakArchiveView &archive,
	const PakSearchRuntime &runtime, stringlist_t *list, const char *pattern,
	bool caseInsensitive);
file_t *OpenPakEntry(const PakArchiveView &archive,
	const PakOpenFileRuntime &runtime, int index);
PakLoadStatus OpenPakArchive(const PakOpenRuntime &runtime,
	const char *filename, PakOpenResult *result);

}
}

#endif
