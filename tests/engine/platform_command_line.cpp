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

static bool TestCAdapterMatchesCppHelper()
{
	return strcmp(Xash_ChangeGameCensoredArgument(), ChangeGameCensoredArgument()) == 0 &&
		Xash_ShouldCensorChangeGameArgument("+load") == 1 &&
		Xash_ShouldCensorChangeGameArgument("-dev") == 0;
}

int main()
{
	if (!TestBlockedChangeGameArguments() ||
		!TestBlockedArgumentsAreCaseInsensitive() ||
		!TestNonBlockedArgumentsArePreserved() ||
		!TestSanitizeOnlyWhenChangingGame() ||
		!TestCAdapterMatchesCppHelper())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
