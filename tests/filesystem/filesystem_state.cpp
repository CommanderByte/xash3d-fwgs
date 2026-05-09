#include <stdlib.h>
#include <string.h>

#include "filesystem/filesystem_state.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestDefaultState()
{
	FilesystemState state;

	return ExpectString(state.rootDir(), "") &&
		ExpectString(state.baseDir(), "") &&
		ExpectString(state.gameDir(), "") &&
		ExpectString(state.readOnlyDir(), "") &&
		ExpectString(state.language(), "") &&
		state.searchPaths() == NULL &&
		state.writePath() == NULL &&
		!state.directPathsEnabled();
}

static bool TestConfiguredState()
{
	searchpath_t *searchPaths = reinterpret_cast<searchpath_t *>(0x10);
	searchpath_t *writePath = reinterpret_cast<searchpath_t *>(0x20);
	FilesystemStateConfig config = {
		"C:/xash/",
		"valve",
		"mod",
		"D:/Half-Life/",
		"french",
		searchPaths,
		writePath,
		true,
	};
	FilesystemState state(config);

	return ExpectString(state.rootDir(), "C:/xash/") &&
		ExpectString(state.baseDir(), "valve") &&
		ExpectString(state.gameDir(), "mod") &&
		ExpectString(state.readOnlyDir(), "D:/Half-Life/") &&
		ExpectString(state.language(), "french") &&
		state.searchPaths() == searchPaths &&
		state.writePath() == writePath &&
		state.directPathsEnabled();
}

static bool TestSettersAndReset()
{
	FilesystemState state;
	searchpath_t *searchPaths = reinterpret_cast<searchpath_t *>(0x30);
	searchpath_t *writePath = reinterpret_cast<searchpath_t *>(0x40);

	state.setRootDir("root");
	state.setBaseDir("base");
	state.setGameDir("game");
	state.setReadOnlyDir("ro");
	state.setLanguage("spanish");
	state.setSearchPaths(searchPaths);
	state.setWritePath(writePath);
	state.setDirectPathsEnabled(true);

	if (!ExpectString(state.rootDir(), "root") ||
		!ExpectString(state.baseDir(), "base") ||
		!ExpectString(state.gameDir(), "game") ||
		!ExpectString(state.readOnlyDir(), "ro") ||
		!ExpectString(state.language(), "spanish") ||
		state.searchPaths() != searchPaths ||
		state.writePath() != writePath ||
		!state.directPathsEnabled())
	{
		return false;
	}

	state.reset();
	return TestDefaultState() &&
		ExpectString(state.rootDir(), "") &&
		state.searchPaths() == NULL;
}

static bool TestNullAndTruncatedStrings()
{
	FilesystemState state;
	char longValue[MAX_STRING + 8];
	for (size_t i = 0; i < sizeof(longValue) - 1; ++i)
		longValue[i] = 'a';
	longValue[sizeof(longValue) - 1] = '\0';

	state.setRootDir(NULL);
	state.setLanguage(longValue);

	if (!ExpectString(state.rootDir(), ""))
		return false;

	return strlen(state.language()) == MAX_STRING - 1;
}

int main()
{
	if (!TestDefaultState() ||
		!TestConfiguredState() ||
		!TestSettersAndReset() ||
		!TestNullAndTruncatedStrings())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
