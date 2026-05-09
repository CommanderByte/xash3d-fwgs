#ifndef XASH_ENGINE_PLATFORM_COMMAND_LINE_HPP
#define XASH_ENGINE_PLATFORM_COMMAND_LINE_HPP

namespace xash
{
namespace engine
{
namespace platform
{

struct CommandLineView
{
	int argc;
	const char **argv;
};

const char *ChangeGameCensoredArgument();
int FindCommandLineArgument(CommandLineView commandLine, const char *argument);
const char *FindCommandLineValue(CommandLineView commandLine, const char *argument);
bool ShouldCensorChangeGameArgument(const char *argument);
const char *SanitizeChangeGameArgument(const char *argument, bool changeGame);

}
}
}

#endif
