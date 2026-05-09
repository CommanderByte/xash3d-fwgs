#ifndef XASH_FILESYSTEM_DIRECTORY_BACKEND_HPP
#define XASH_FILESYSTEM_DIRECTORY_BACKEND_HPP

#include "filesystem/search_path_backend.hpp"

namespace xash
{
namespace filesystem
{

struct DirectoryBackendOps
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

class DirectoryBackend final : public ISearchPathBackend
{
public:
	explicit DirectoryBackend(const SearchPathMetadata &metadata);
	DirectoryBackend(const SearchPathMetadata &metadata,
		const DirectoryBackendOps &ops);

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
	DirectoryBackendOps m_ops;
	bool m_hasOps;
};

enum DirectoryEntryState
{
	DirectoryEntryEmpty = 0,
	DirectoryEntryNotScanned = -1,
	DirectoryEntryCaseInsensitive = -2,
};

struct DirectoryEntry
{
	string name;
	int entryCount;
	DirectoryEntry *entries;
	void *backend;
};

struct DirectoryCaseRuntime
{
	void *context;
	void *(*alloc)(void *context, size_t size, bool clear);
	void (*free)(void *context, void *memory);
	bool (*folderExists)(void *context, const char *path);
	bool (*fileExists)(void *context, const char *path);
	bool (*fileOrFolderExists)(void *context, const char *path);
	bool (*isDirectoryCaseSensitive)(void *context, const char *path);
	stringlist_t *(*listCreate)(void *context);
	void (*listDirectory)(void *context, stringlist_t *list, const char *path,
		bool dirsOnly);
	void (*listDestroy)(void *context, stringlist_t *list);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*overflow)(void *context, const char *path, const char *operation);
};

struct DirectorySearchRuntime
{
	void *context;
	DirectoryCaseRuntime caseRuntime;
	bool (*matchPattern)(void *context, const char *text, const char *pattern,
		bool caseInsensitive);
	int (*stringCount)(void *context, stringlist_t *list);
	const char *(*stringAt)(void *context, stringlist_t *list, int index);
	void (*append)(void *context, stringlist_t *list, const char *text);
};

struct DirectoryOpenRuntime
{
	void *context;
	DirectoryCaseRuntime caseRuntime;
	file_t *(*openSystem)(void *context, const char *path, const char *mode);
	void (*setSearchPath)(void *context, file_t *file, void *searchPath);
};

void FreeDirectoryEntries(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime);
void PopulateDirectoryEntries(DirectoryEntry *dir, const char *path,
	const DirectoryCaseRuntime &runtime);
int FindDirectoryEntry(DirectoryEntry *dir, const char *name);
bool FixDirectoryFileCase(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime, const char *path, char *dst,
	size_t dstSize, bool createPath);
int FindFileInDirectory(DirectoryEntry *dir,
	const DirectoryCaseRuntime &runtime, const char *searchPath,
	const char *path, char *fixedName, size_t fixedNameSize);
void SearchDirectory(DirectoryEntry *dir, const DirectorySearchRuntime &runtime,
	stringlist_t *list, const char *pattern, bool caseInsensitive);
file_t *OpenDirectoryFile(DirectoryEntry *dir,
	const DirectoryOpenRuntime &runtime, void *searchPath, const char *rootPath,
	const char *filename, const char *mode);

}
}

#endif
