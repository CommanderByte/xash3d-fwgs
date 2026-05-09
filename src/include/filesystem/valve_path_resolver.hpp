#ifndef XASH_FILESYSTEM_VALVE_PATH_RESOLVER_HPP
#define XASH_FILESYSTEM_VALVE_PATH_RESOLVER_HPP

#include <stddef.h>

namespace xash
{
namespace filesystem
{

struct ValvePathContext
{
	const char *rootDir;
	const char *gameDir;
	const char *writePath;
};

class ValvePathResolver
{
public:
	static bool isGameDirectoryId(const char *id);
	static const char *resolveDirectory(char *buffer, size_t size,
		const char *id, const ValvePathContext &context);
};

}
}

#endif
