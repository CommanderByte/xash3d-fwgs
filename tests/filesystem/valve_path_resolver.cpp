#include <stdlib.h>
#include <string.h>

#include "filesystem/valve_path_resolver.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestGameDirectoryIds()
{
	return ValvePathResolver::isGameDirectoryId("GAME") &&
		ValvePathResolver::isGameDirectoryId("GAMECONFIG") &&
		ValvePathResolver::isGameDirectoryId("GAMEDOWNLOAD") &&
		!ValvePathResolver::isGameDirectoryId("ROOT") &&
		!ValvePathResolver::isGameDirectoryId(NULL);
}

static bool TestDirectoryResolution()
{
	char buffer[64];
	ValvePathContext context = {
		"C:/xash",
		"valve",
		"C:/xash/valve"
	};

	return ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "GAME", context), "valve") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "GAMEDOWNLOAD", context),
			"valve_downloads") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "GAMECONFIG", context),
			"C:/xash/valve") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "PLATFORM", context), "platform") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "CONFIG", context), "platform/config") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), "ROOT", context), "C:/xash") &&
		ExpectString(ValvePathResolver::resolveDirectory(
			buffer, sizeof(buffer), NULL, context), "C:/xash");
}

static bool TestDownloadedDirectoryTruncatesSafely()
{
	char buffer[8];
	ValvePathContext context = {
		"root",
		"longgame",
		"write"
	};

	return ExpectString(ValvePathResolver::resolveDirectory(
		buffer, sizeof(buffer), "GAMEDOWNLOAD", context), "longgam");
}

int main()
{
	if (!TestGameDirectoryIds() ||
		!TestDirectoryResolution() ||
		!TestDownloadedDirectoryTruncatesSafely())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
