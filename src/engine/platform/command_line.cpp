#include "engine/platform/command_line.hpp"
#include "engine/platform/command_line_adapter.h"

namespace xash
{
namespace engine
{
namespace platform
{

namespace
{

const char *const kCensoredArgument = "censored";
const char *const kBlockedChangeGameArguments[] =
{
	"-game",
	"+game",
	"+map",
	"+load",
	"+changelevel",
};

char ToLowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value - 'A' + 'a');
	return value;
}

bool EqualsNoCase(const char *left, const char *right)
{
	if (!left || !right)
		return false;

	while (*left && *right)
	{
		if (ToLowerAscii(*left) != ToLowerAscii(*right))
			return false;

		++left;
		++right;
	}

	return *left == '\0' && *right == '\0';
}

}

const char *ChangeGameCensoredArgument()
{
	return kCensoredArgument;
}

int FindCommandLineArgument(CommandLineView commandLine, const char *argument)
{
	if (!argument || commandLine.argc <= 1 || !commandLine.argv)
		return 0;

	for (int i = 1; i < commandLine.argc; ++i)
	{
		if (!commandLine.argv[i])
			continue;

		if (EqualsNoCase(argument, commandLine.argv[i]))
			return i;
	}

	return 0;
}

const char *FindCommandLineValue(CommandLineView commandLine, const char *argument)
{
	const int argumentIndex = FindCommandLineArgument(commandLine, argument);

	if (argumentIndex < 1 || argumentIndex + 1 >= commandLine.argc)
		return nullptr;

	return commandLine.argv[argumentIndex + 1];
}

bool ShouldCensorChangeGameArgument(const char *argument)
{
	for (const char *blocked : kBlockedChangeGameArguments)
	{
		if (EqualsNoCase(argument, blocked))
			return true;
	}

	return false;
}

const char *SanitizeChangeGameArgument(const char *argument, bool changeGame)
{
	if (changeGame && ShouldCensorChangeGameArgument(argument))
		return ChangeGameCensoredArgument();

	return argument;
}

}
}
}

extern "C" const char *Xash_ChangeGameCensoredArgument(void)
{
	return xash::engine::platform::ChangeGameCensoredArgument();
}

extern "C" int Xash_FindCommandLineArgument(int argc, const char **argv, const char *argument)
{
	const xash::engine::platform::CommandLineView commandLine{ argc, argv };
	return xash::engine::platform::FindCommandLineArgument(commandLine, argument);
}

extern "C" const char *Xash_FindCommandLineValue(int argc, const char **argv, const char *argument)
{
	const xash::engine::platform::CommandLineView commandLine{ argc, argv };
	return xash::engine::platform::FindCommandLineValue(commandLine, argument);
}

extern "C" int Xash_ShouldCensorChangeGameArgument(const char *argument)
{
	return xash::engine::platform::ShouldCensorChangeGameArgument(argument) ? 1 : 0;
}
