#include <stdlib.h>
#include <string.h>

#include "engine/platform/command_line.hpp"
#include "engine/platform/command_line_adapter.h"

using namespace xash::engine::platform;

static bool TestBlockedChangeGameArguments()
{
	return ShouldCensorChangeGameArgument("-game") &&
		ShouldCensorChangeGameArgument("+game") &&
		ShouldCensorChangeGameArgument("+map") &&
		ShouldCensorChangeGameArgument("+load") &&
		ShouldCensorChangeGameArgument("+changelevel");
}

static bool TestBlockedArgumentsAreCaseInsensitive()
{
	return ShouldCensorChangeGameArgument("-GAME") &&
		ShouldCensorChangeGameArgument("+Map") &&
		ShouldCensorChangeGameArgument("+ChangeLevel");
}

static bool TestNonBlockedArgumentsArePreserved()
{
	return !ShouldCensorChangeGameArgument(nullptr) &&
		!ShouldCensorChangeGameArgument("") &&
		!ShouldCensorChangeGameArgument("-dev") &&
		!ShouldCensorChangeGameArgument("map") &&
		!ShouldCensorChangeGameArgument("+map_background");
}

static bool TestSanitizeOnlyWhenChangingGame()
{
	const char *argument = "+map";

	return SanitizeChangeGameArgument(argument, false) == argument &&
		strcmp(SanitizeChangeGameArgument(argument, true), "censored") == 0;
}

static bool TestFindArgumentUsesLegacyRules()
{
	const char *argv[] =
	{
		"xash3d",
		"-dev",
		"2",
		nullptr,
		"+MAP",
		"c1a0d",
		"-dev",
		"3",
	};
	const CommandLineView commandLine{ 8, argv };

	return FindCommandLineArgument(commandLine, "xash3d") == 0 &&
		FindCommandLineArgument(commandLine, "-DEV") == 1 &&
		FindCommandLineArgument(commandLine, "+map") == 4 &&
		FindCommandLineArgument(commandLine, "-missing") == 0 &&
		FindCommandLineArgument({ 8, nullptr }, "-dev") == 0 &&
		FindCommandLineArgument({ -1, argv }, "-dev") == 0 &&
		FindCommandLineArgument(commandLine, nullptr) == 0;
}

static bool TestFindValueIsBoundsSafe()
{
	const char *argv[] =
	{
		"xash3d",
		"-dev",
		"2",
		"+map",
		"c1a0d",
		"-game",
		nullptr,
		"-last",
	};
	const CommandLineView commandLine{ 8, argv };

	return strcmp(FindCommandLineValue(commandLine, "-dev"), "2") == 0 &&
		strcmp(FindCommandLineValue(commandLine, "+MAP"), "c1a0d") == 0 &&
		FindCommandLineValue(commandLine, "-game") == nullptr &&
		FindCommandLineValue(commandLine, "-last") == nullptr &&
		FindCommandLineValue(commandLine, "-missing") == nullptr;
}

static bool TestCAdapterMatchesCppHelper()
{
	const char *argv[] =
	{
		"xash3d",
		"-dev",
		"2",
	};

	return strcmp(Xash_ChangeGameCensoredArgument(), ChangeGameCensoredArgument()) == 0 &&
		Xash_FindCommandLineArgument(3, argv, "-DEV") == 1 &&
		strcmp(Xash_FindCommandLineValue(3, argv, "-dev"), "2") == 0 &&
		Xash_FindCommandLineValue(2, argv, "-dev") == nullptr &&
		Xash_ShouldCensorChangeGameArgument("+load") == 1 &&
		Xash_ShouldCensorChangeGameArgument("-dev") == 0;
}

int main()
{
	if (!TestBlockedChangeGameArguments() ||
		!TestBlockedArgumentsAreCaseInsensitive() ||
		!TestNonBlockedArgumentsArePreserved() ||
		!TestSanitizeOnlyWhenChangingGame() ||
		!TestFindArgumentUsesLegacyRules() ||
		!TestFindValueIsBoundsSafe() ||
		!TestCAdapterMatchesCppHelper())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
