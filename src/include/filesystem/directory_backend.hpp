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

}
}

#endif
