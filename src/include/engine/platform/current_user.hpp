#ifndef XASH_ENGINE_PLATFORM_CURRENT_USER_HPP
#define XASH_ENGINE_PLATFORM_CURRENT_USER_HPP

namespace xash
{
namespace engine
{
namespace platform
{

const char *DefaultCurrentUserName();
bool IsUsableCurrentUserName(const char *name);
const char *SelectCurrentUserName(const char *candidate);

}
}
}

#endif
