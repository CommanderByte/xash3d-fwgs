#ifndef XASH_FILESYSTEM_PATH_POLICY_HPP
#define XASH_FILESYSTEM_PATH_POLICY_HPP

#include <stddef.h>

namespace xash
{
namespace filesystem
{

enum class PathRejection
{
	Allowed = 0,
	NonPortable = 1,
	OutsideGameDirectory = 2,
};

class PathPolicy
{
public:
	static PathRejection checkPath(const char *path, bool directPathsEnabled);
	static const char *stripDirectRelativePrefix(const char *path);
	static bool isWriteMode(const char *mode);

private:
	static bool empty(const char *text);
	static bool contains(const char *text, const char *pattern);
	static bool startsWith(const char *text, const char *prefix);
};

}
}

#endif
