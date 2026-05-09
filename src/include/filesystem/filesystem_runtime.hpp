#ifndef XASH_FILESYSTEM_FILESYSTEM_RUNTIME_HPP
#define XASH_FILESYSTEM_FILESYSTEM_RUNTIME_HPP

#include <stddef.h>

#include "filesystem/filesystem_state.hpp"

namespace xash
{
namespace filesystem
{

struct SearchPathListOps
{
	void *context;
	searchpath_t *(*next)(void *context, searchpath_t *path);
	void (*setNext)(void *context, searchpath_t *path, searchpath_t *next);
	bool (*isStatic)(void *context, searchpath_t *path);
	void (*close)(void *context, searchpath_t *path);
	void (*freePath)(void *context, searchpath_t *path);
	void (*markGamesNotAdded)(void *context);
};

struct SearchPathClearResult
{
	searchpath_t *searchPaths;
	searchpath_t *writePath;
	int keptCount;
	int removedCount;
};

struct FileHandleMemoryOps
{
	void *context;
	file_t *(*alloc)(void *context, bool clear);
	void (*free)(void *context, file_t *file);
};

struct FilesystemRescanPlan
{
	uint32_t mountFlags;
	bool localizationEnabled;
	const char *language;
};

class FilesystemRuntime
{
public:
	FilesystemRuntime();

	void reset();
	void configure(const FilesystemStateConfig &config);

	FilesystemState &state();
	const FilesystemState &state() const;

	searchpath_t *searchPaths() const;
	void setSearchPaths(searchpath_t *value);
	void prependSearchPath(searchpath_t *path,
		const SearchPathListOps &ops);

	searchpath_t *writePath() const;
	void setWritePath(searchpath_t *value);
	bool isWritePathMounted(const SearchPathListOps &ops) const;

	SearchPathClearResult clearDynamicSearchPaths(
		const SearchPathListOps &ops);

	file_t *allocateFile(const FileHandleMemoryOps &ops,
		bool clear = true) const;
	void freeFile(const FileHandleMemoryOps &ops, file_t *file) const;

	FilesystemRescanPlan beginRescan(uint32_t flags,
		const char *language, uint32_t allowedMountFlags,
		uint32_t localizationFlag);

private:
	FilesystemState m_state;
};

}
}

#endif
