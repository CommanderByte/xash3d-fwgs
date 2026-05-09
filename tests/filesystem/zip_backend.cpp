#include <stdlib.h>
#include <string.h>

#include "filesystem/zip_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"extras.pk3",
		SearchPathBackendType::Zip,
		4,
		4,
		"game pk3"
	};
	return metadata;
}

static bool TestZipBackendMetadata()
{
	ZipBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "extras.pk3") &&
		metadata.type == SearchPathBackendType::Zip &&
		metadata.flags == 4 &&
		metadata.order == 4 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game pk3");
}

static bool TestZipBackendPrintInfo()
{
	ZipBackend backend(TestMetadata());
	char output[7];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "extras"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestZipBackendDefaultOperations()
{
	ZipBackend backend(TestMetadata());

	if (backend.openFile("stored.txt", "rb", 0))
		return false;

	if (backend.fileTime("stored.txt") != -1)
		return false;

	if (backend.findFile("stored.txt", NULL, 0) != -1)
		return false;

	if (backend.loadFile("stored.txt", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

int main()
{
	if (!TestZipBackendMetadata() ||
		!TestZipBackendPrintInfo() ||
		!TestZipBackendDefaultOperations())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
