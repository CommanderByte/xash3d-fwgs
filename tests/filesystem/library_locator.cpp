#include <stdlib.h>
#include <string.h>

#include "filesystem/library_locator.hpp"

using namespace xash::filesystem;

static bool ExpectNormalize(
	const char *requestedName,
	const char *gameFolder,
	const char *fallbackGameFolder,
	const char *expected)
{
	char output[128];
	LibraryShortPathConfig config = {};

	config.requestedName = requestedName;
	config.gameFolder = gameFolder;
	config.fallbackGameFolder = fallbackGameFolder;
	config.defaultExtension = ".dll";

	if (!LibraryLocator::normalizeShortPath(config, output, sizeof(output)))
		return false;

	return strcmp(output, expected) == 0;
}

static bool TestShortPathNormalization()
{
	return ExpectNormalize("DLLS\\HL", "valve", "valve", "dlls/hl.dll") &&
		ExpectNormalize("cl_dlls/client.dll", "valve", "valve",
			"cl_dlls/client.dll");
}

static bool TestRelativeGamePrefix()
{
	return LibraryLocator::stripRelativeGamePrefix(
			"../valve/dlls/hl.dll", "valve") == strlen("../valve/") &&
		LibraryLocator::stripRelativeGamePrefix(
			"..\\MOD\\cl_dlls\\client.dll", "mod") == strlen("..\\MOD\\") &&
		LibraryLocator::stripRelativeGamePrefix(
			"../other/dlls/hl.dll", "valve") == 0;
}

static bool TestRelativeNormalization()
{
	return ExpectNormalize("../valve/DLLS/HL.DLL", "mod", "valve",
			"dlls/hl.dll") &&
		ExpectNormalize("..\\mod\\cl_dlls\\CLIENT", "mod", "valve",
			"cl_dlls/client.dll");
}

static bool TestRejectsInvalidOutput()
{
	char output[4];
	LibraryShortPathConfig config = {};

	config.requestedName = "dlls/toolong";
	config.gameFolder = "valve";
	config.fallbackGameFolder = "valve";
	config.defaultExtension = ".dll";

	return !LibraryLocator::normalizeShortPath(config, output, sizeof(output)) &&
		!LibraryLocator::normalizeShortPath(config, nullptr, sizeof(output));
}

static bool TestEncryptionExtensionGate()
{
	return LibraryLocator::shouldCheckEncryption("dlls/hl.dll", "dll") &&
		LibraryLocator::shouldCheckEncryption("dlls/hl.DLL", ".dll") &&
		!LibraryLocator::shouldCheckEncryption("cl_dlls/client.so", "dll") &&
		!LibraryLocator::shouldCheckEncryption("dlls/noext", "dll");
}

int main()
{
	if (!TestShortPathNormalization() ||
		!TestRelativeGamePrefix() ||
		!TestRelativeNormalization() ||
		!TestRejectsInvalidOutput() ||
		!TestEncryptionExtensionGate())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
