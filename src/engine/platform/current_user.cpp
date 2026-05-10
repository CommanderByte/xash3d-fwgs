#include "engine/platform/current_user.hpp"

namespace xash
{
namespace engine
{
namespace platform
{

const char *DefaultCurrentUserName()
{
	return "Player";
}

bool IsUsableCurrentUserName(const char *name)
{
	return name && name[0] != '\0';
}

const char *SelectCurrentUserName(const char *candidate)
{
	if (IsUsableCurrentUserName(candidate))
		return candidate;

	return DefaultCurrentUserName();
}

}
}
}
