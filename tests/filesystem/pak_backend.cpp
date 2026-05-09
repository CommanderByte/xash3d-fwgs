#include <stdlib.h>
#include <string.h>

#include "filesystem/pak_backend.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static SearchPathMetadata TestMetadata()
{
	SearchPathMetadata metadata = {
		"pak0.pak",
		SearchPathBackendType::Pak,
		4,
		2,
		"game archive"
	};
	return metadata;
}

static bool TestPakBackendMetadata()
{
	PakBackend backend(TestMetadata());
	const SearchPathMetadata &metadata = backend.metadata();

	return metadata.source &&
		ExpectString(metadata.source, "pak0.pak") &&
		metadata.type == SearchPathBackendType::Pak &&
		metadata.flags == 4 &&
		metadata.order == 2 &&
		metadata.mountReason &&
		ExpectString(metadata.mountReason, "game archive");
}

static bool TestPakBackendPrintInfo()
{
	PakBackend backend(TestMetadata());
	char output[8];
	backend.printInfo(output, sizeof(output));

	if (!ExpectString(output, "pak0.pa"))
		return false;

	char empty[1];
	backend.printInfo(empty, sizeof(empty));
	return ExpectString(empty, "");
}

static bool TestPakBackendDefaultOperations()
{
	PakBackend backend(TestMetadata());

	if (backend.openFile("progs.dat", "rb", 0))
		return false;

	if (backend.fileTime("progs.dat") != -1)
		return false;

	if (backend.findFile("progs.dat", NULL, 0) != -1)
		return false;

	if (backend.loadFile("progs.dat", 0, NULL, NULL, NULL))
		return false;

	backend.search(NULL, "*", false);
	return true;
}

int main()
{
	if (!TestPakBackendMetadata() ||
		!TestPakBackendPrintInfo() ||
		!TestPakBackendDefaultOperations())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
