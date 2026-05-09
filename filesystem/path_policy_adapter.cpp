#include "path_policy_adapter.h"

#include "filesystem/path_policy.hpp"

extern "C" {

int FS_PathPolicy_CheckPath(const char *path, int directPathsEnabled)
{
	return static_cast<int>(
		xash::filesystem::PathPolicy::checkPath(path,
			directPathsEnabled != 0));
}

const char *FS_PathPolicy_StripDirectRelativePrefix(const char *path)
{
	return xash::filesystem::PathPolicy::stripDirectRelativePrefix(path);
}

int FS_PathPolicy_IsWriteMode(const char *mode)
{
	return xash::filesystem::PathPolicy::isWriteMode(mode) ? 1 : 0;
}

}
