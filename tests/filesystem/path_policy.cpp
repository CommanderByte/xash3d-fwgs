#include <stdlib.h>
#include <string.h>

#include "filesystem/path_policy.hpp"

using namespace xash::filesystem;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestPathRejection()
{
	return PathPolicy::checkPath(NULL, false) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("", false) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("sound/player.wav", false) ==
			PathRejection::Allowed &&
		PathPolicy::checkPath("c:/hl/valve/pak0.pak", false) ==
			PathRejection::NonPortable &&
		PathPolicy::checkPath("models//barney.mdl", false) ==
			PathRejection::NonPortable &&
		PathPolicy::checkPath("../valve/config.cfg", false) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("maps/..hidden.bsp", false) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("/absolute/path", false) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("save/.hidden", false) ==
			PathRejection::OutsideGameDirectory;
}

static bool TestDirectPathMode()
{
	return PathPolicy::checkPath("", true) ==
			PathRejection::OutsideGameDirectory &&
		PathPolicy::checkPath("../valve/config.cfg", true) ==
			PathRejection::Allowed &&
		PathPolicy::checkPath("c:/hl/valve/pak0.pak", true) ==
			PathRejection::Allowed &&
		PathPolicy::checkPath("models//barney.mdl", true) ==
			PathRejection::Allowed;
}

static bool TestDirectRelativeStrip()
{
	return ExpectString(PathPolicy::stripDirectRelativePrefix(NULL), "") &&
		ExpectString(PathPolicy::stripDirectRelativePrefix("../config.cfg"),
			"config.cfg") &&
		ExpectString(PathPolicy::stripDirectRelativePrefix("..\\config.cfg"),
			"..\\config.cfg") &&
		ExpectString(PathPolicy::stripDirectRelativePrefix("../../config.cfg"),
			"../config.cfg") &&
		ExpectString(PathPolicy::stripDirectRelativePrefix("config.cfg"),
			"config.cfg");
}

static bool TestWriteModes()
{
	return !PathPolicy::isWriteMode(NULL) &&
		!PathPolicy::isWriteMode("") &&
		!PathPolicy::isWriteMode("rb") &&
		PathPolicy::isWriteMode("wb") &&
		PathPolicy::isWriteMode("ab") &&
		PathPolicy::isWriteMode("eb") &&
		PathPolicy::isWriteMode("r+b");
}

int main()
{
	if (!TestPathRejection() ||
		!TestDirectPathMode() ||
		!TestDirectRelativeStrip() ||
		!TestWriteModes())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
