#include "filesystem/valve_path_resolver.hpp"

#include <string.h>

namespace xash
{
namespace filesystem
{

namespace
{

const char *SafeString(const char *text)
{
	return text ? text : "";
}

bool StringEquals(const char *lhs, const char *rhs)
{
	return strcmp(SafeString(lhs), rhs) == 0;
}

const char *CopyDownloadedDirectory(char *buffer, size_t size,
	const char *gameDir)
{
	static const char kDownloadedSuffix[] = "_downloads";

	if (!buffer || size == 0)
		return "";

	const char *game = SafeString(gameDir);
	size_t i = 0;

	for (; i + 1 < size && game[i]; ++i)
		buffer[i] = game[i];

	for (size_t suffix = 0; i + 1 < size && kDownloadedSuffix[suffix];
		 ++i, ++suffix)
	{
		buffer[i] = kDownloadedSuffix[suffix];
	}

	buffer[i] = '\0';
	return buffer;
}

}

bool ValvePathResolver::isGameDirectoryId(const char *id)
{
	return StringEquals(id, "GAME") ||
		StringEquals(id, "GAMECONFIG") ||
		StringEquals(id, "GAMEDOWNLOAD");
}

const char *ValvePathResolver::resolveDirectory(char *buffer, size_t size,
	const char *id, const ValvePathContext &context)
{
	if (StringEquals(id, "GAME"))
		return SafeString(context.gameDir);

	if (StringEquals(id, "GAMEDOWNLOAD"))
		return CopyDownloadedDirectory(buffer, size, context.gameDir);

	if (StringEquals(id, "GAMECONFIG"))
		return SafeString(context.writePath);

	if (StringEquals(id, "PLATFORM"))
		return "platform";

	if (StringEquals(id, "CONFIG"))
		return "platform/config";

	return SafeString(context.rootDir);
}

}
}
