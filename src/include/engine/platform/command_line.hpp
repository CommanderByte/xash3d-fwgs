#ifndef XASH_ENGINE_PLATFORM_COMMAND_LINE_HPP
#define XASH_ENGINE_PLATFORM_COMMAND_LINE_HPP

namespace xash
{
namespace engine
{
namespace platform
{

const char *ChangeGameCensoredArgument();
bool ShouldCensorChangeGameArgument(const char *argument);
const char *SanitizeChangeGameArgument(const char *argument, bool changeGame);

}
}
}

#endif
