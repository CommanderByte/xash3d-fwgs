#ifndef XASH_FILESYSTEM_SEARCH_PATH_BACKEND_HPP
#define XASH_FILESYSTEM_SEARCH_PATH_BACKEND_HPP

#include <stddef.h>

#include "xash3d_types.h"

typedef struct file_s file_t;
typedef struct stringlist_s stringlist_t;
typedef struct searchpath_s searchpath_t;

namespace xash
{
namespace filesystem
{

enum class SearchPathBackendType
{
	Directory,
	Pak,
	Wad,
	Zip,
	Pk3Directory,
	AndroidAssets,
	Unknown,
};

struct SearchPathMetadata
{
	const char *source;
	SearchPathBackendType type;
	int flags;
	unsigned order;
	const char *mountReason;
};

class ISearchPathBackend
{
public:
	virtual ~ISearchPathBackend() = default;

	virtual const SearchPathMetadata &metadata() const = 0;
	virtual void printInfo(char *dst, size_t size) const = 0;
	virtual void close() = 0;
	virtual file_t *openFile(const char *path, const char *mode, int index) = 0;
	virtual int fileTime(const char *path) const = 0;
	virtual int findFile(const char *path, char *fixedName, size_t len) = 0;
	virtual void search(stringlist_t *list, const char *pattern,
		bool caseInsensitive) = 0;
	virtual byte *loadFile(const char *path, int index,
		fs_offset_t *fileSize, void *(*alloc)(size_t),
		void (*freeFn)(void *));
};

const char *SearchPathBackendTypeName(SearchPathBackendType type);

}
}

#endif
