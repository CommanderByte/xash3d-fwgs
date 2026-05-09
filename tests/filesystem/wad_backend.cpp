#include <stdlib.h>
#include <string.h>

#include "filesystem/wad_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"halflife.wad",
		SearchPathBackendType::Wad,
		4,
		3,
		"game wad"
	};
	return metadata;
}

static bool TestWadBackendMetadata()
{
	WadBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "halflife.wad") &&
		metadata.type == SearchPathBackendType::Wad &&
		metadata.flags == 4 &&
		metadata.order == 3 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game wad");
}

static bool TestWadBackendPrintInfo()
{
	WadBackend backend(TestMetadata());
	char output[9];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "halflife"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestWadBackendDefaultOperations()
{
	WadBackend backend(TestMetadata());

	if (backend.openFile("probe.txt", "rb", 0))
		return false;

	if (backend.fileTime("probe.txt") != -1)
		return false;

	if (backend.findFile("probe.txt", NULL, 0) != -1)
		return false;

	if (backend.loadFile("probe.txt", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

int main()
{
	if (!TestWadBackendMetadata() ||
		!TestWadBackendPrintInfo() ||
		!TestWadBackendDefaultOperations())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
