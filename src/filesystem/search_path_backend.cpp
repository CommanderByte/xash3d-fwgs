#include "filesystem/search_path_backend.hpp"

namespace xash
{
namespace filesystem
{

byte *ISearchPathBackend::loadFile(const char *path, int index,
	fs_offset_t *fileSize, void *(*alloc)(size_t), void (*freeFn)(void *))
{
	(void)path;
	(void)index;
	(void)fileSize;
	(void)alloc;
	(void)freeFn;

	return NULL;
}

const char *SearchPathBackendTypeName(SearchPathBackendType type)
{
	switch (type)
	{
	case SearchPathBackendType::Directory:
		return "directory";
	case SearchPathBackendType::Pak:
		return "pak";
	case SearchPathBackendType::Wad:
		return "wad";
	case SearchPathBackendType::Zip:
		return "zip";
	case SearchPathBackendType::Pk3Directory:
		return "pk3dir";
	case SearchPathBackendType::AndroidAssets:
		return "android_assets";
	case SearchPathBackendType::Unknown:
	default:
		return "unknown";
	}
}

}
}
