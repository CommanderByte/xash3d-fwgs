#ifndef XASH_FILESYSTEM_LIBRARY_LOCATOR_HPP
#define XASH_FILESYSTEM_LIBRARY_LOCATOR_HPP

#include <stddef.h>

namespace xash
{
namespace filesystem
{

struct LibraryShortPathConfig
{
	const char *requestedName;
	const char *gameFolder;
	const char *fallbackGameFolder;
	const char *defaultExtension;
};

class LibraryLocator
{
public:
	static bool normalizeShortPath(
		const LibraryShortPathConfig &config,
		char *output,
		size_t outputSize);
	static size_t stripRelativeGamePrefix(
		const char *path,
		const char *gameFolder);
	static bool shouldCheckEncryption(
		const char *shortPath,
		const char *libraryExtension);

private:
	static bool startsWithDotDot(const char *path);
	static bool isSlash(char value);
	static char toLower(char value);
	static bool hasExtension(const char *path);
	static bool extensionEquals(
		const char *path,
		const char *extension);
	static bool appendDefaultExtension(
		char *output,
		size_t outputSize,
		const char *extension);
};

}
}

#endif
