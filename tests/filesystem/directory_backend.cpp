#include <stdlib.h>
#include <string.h>

#include "filesystem/directory_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"valve/",
		SearchPathBackendType::Directory,
		4,
		12,
		"gamefolder"
	};
	return metadata;
}

static bool TestSearchPathBackendTypeNames()
{
	return ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Directory), "directory") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Pak), "pak") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Wad), "wad") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Zip), "zip") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Pk3Directory), "pk3dir") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::AndroidAssets), "android_assets") &&
		ExpectString(SearchPathBackendTypeName(SearchPathBackendType::Unknown), "unknown");
}

static bool TestDirectoryBackendMetadata()
{
	DirectoryBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "valve/") &&
		metadata.type == SearchPathBackendType::Directory &&
		metadata.flags == 4 &&
		metadata.order == 12 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "gamefolder");
}

static bool TestDirectoryBackendPrintInfo()
{
	DirectoryBackend backend(TestMetadata());
	char output[8];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "valve/"))
		return false;

	char truncated[4];
	backend.printInfo(truncated, sizeof(truncated));

	return ExpectString(truncated, "val");
}

static bool TestDirectoryBackendDefaultOperations()
{
	DirectoryBackend backend(TestMetadata());

	if (backend.openFile("maps/c0a0.bsp", "rb", 0))
		return false;

	if (backend.fileTime("maps/c0a0.bsp") != -1)
		return false;

	if (backend.findFile("maps/c0a0.bsp", NULL, 0) != -1)
		return false;

	if (backend.loadFile("maps/c0a0.bsp", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

int main()
{
	if (!TestSearchPathBackendTypeNames() ||
		!TestDirectoryBackendMetadata() ||
		!TestDirectoryBackendPrintInfo() ||
		!TestDirectoryBackendDefaultOperations())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
