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

extern "C" int Xash_ShouldCensorChangeGameArgument(const char *argument)
{
	return xash::engine::platform::ShouldCensorChangeGameArgument(argument) ? 1 : 0;
}
