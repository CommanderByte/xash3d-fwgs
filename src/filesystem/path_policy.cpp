#include "filesystem/path_policy.hpp"

namespace xash
{
namespace filesystem
{

PathRejection PathPolicy::checkPath(const char *path, bool directPathsEnabled)
{
	if (empty(path))
		return PathRejection::OutsideGameDirectory;

	if (directPathsEnabled)
		return PathRejection::Allowed;

	if (contains(path, ":"))
		return PathRejection::NonPortable;

	if (contains(path, "//"))
		return PathRejection::NonPortable;

	if (contains(path, ".."))
		return PathRejection::OutsideGameDirectory;

	if (path[0] == '/')
		return PathRejection::OutsideGameDirectory;

	if (contains(path, "/."))
		return PathRejection::OutsideGameDirectory;

	return PathRejection::Allowed;
}

const char *PathPolicy::stripDirectRelativePrefix(const char *path)
{
	if (!path)
		return "";

	if (startsWith(path, "../"))
		return path + 3;

	return path;
}

bool PathPolicy::isWriteMode(const char *mode)
{
	if (empty(mode))
		return false;

	if (mode[0] == 'w' || mode[0] == 'a' || mode[0] == 'e')
		return true;

	return contains(mode, "+");
}

bool PathPolicy::empty(const char *text)
{
	return !text || !text[0];
}

bool PathPolicy::contains(const char *text, const char *pattern)
{
	if (empty(text) || empty(pattern))
		return false;

	for (size_t i = 0; text[i]; ++i)
	{
		size_t j = 0;
		while (pattern[j] && text[i + j] == pattern[j])
			++j;

		if (!pattern[j])
			return true;
	}

	return false;
}

bool PathPolicy::startsWith(const char *text, const char *prefix)
{
	if (empty(text) || empty(prefix))
		return false;

	for (size_t i = 0; prefix[i]; ++i)
	{
		if (text[i] != prefix[i])
			return false;
	}

	return true;
}

}
}
