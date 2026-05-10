#include <stdlib.h>
#include <string.h>

#include "engine/platform/current_user.hpp"

using namespace xash::engine::platform;

static bool TestDefaultUserName()
{
	return strcmp(DefaultCurrentUserName(), "Player") == 0;
}

static bool TestUsableUserName()
{
	return IsUsableCurrentUserName("player") &&
		IsUsableCurrentUserName(" ") &&
		!IsUsableCurrentUserName("") &&
		!IsUsableCurrentUserName(nullptr);
}

static bool TestSelectCurrentUserName()
{
	const char *name = "Barney";

	return SelectCurrentUserName(name) == name &&
		strcmp(SelectCurrentUserName(""), "Player") == 0 &&
		strcmp(SelectCurrentUserName(nullptr), "Player") == 0;
}

int main()
{
	if (!TestDefaultUserName() ||
		!TestUsableUserName() ||
		!TestSelectCurrentUserName())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
