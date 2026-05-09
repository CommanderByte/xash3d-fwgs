#include "filesystem/library_locator.hpp"

#include <string.h>

namespace xash
{
namespace filesystem
{

namespace
{

bool StringEmpty(const char *value)
{
	return value == nullptr || value[0] == '\0';
}

}

bool LibraryLocator::normalizeShortPath(
	const LibraryShortPathConfig &config,
	char *output,
	size_t outputSize)
{
	if (StringEmpty(config.requestedName) || output == nullptr || outputSize == 0)
		return false;

	size_t start = 0;
	if (startsWithDotDot(config.requestedName))
	{
		start = stripRelativeGamePrefix(config.requestedName, config.gameFolder);
		if (start == 0)
			start = stripRelativeGamePrefix(config.requestedName, config.fallbackGameFolder);
	}

	size_t out = 0;
	for (const char *in = config.requestedName + start; *in != '\0'; ++in)
	{
		if (out + 1 >= outputSize)
			return false;

		output[out++] = isSlash(*in) ? '/' : toLower(*in);
	}

	output[out] = '\0';

	if (!hasExtension(output))
		return appendDefaultExtension(output, outputSize, config.defaultExtension);

	return true;
}

size_t LibraryLocator::stripRelativeGamePrefix(
	const char *path,
	const char *gameFolder)
{
	if (StringEmpty(path) || StringEmpty(gameFolder))
		return 0;

	if (path[0] != '.' || path[1] != '.' || !isSlash(path[2]))
		return 0;

	size_t i = 3;
	for (size_t j = 0; gameFolder[j] != '\0'; ++i, ++j)
	{
		if (toLower(path[i]) != toLower(gameFolder[j]))
			return 0;
	}

	if (!isSlash(path[i]))
		return 0;

	return i + 1;
}

bool LibraryLocator::startsWithDotDot(const char *path)
{
	return path != nullptr && path[0] == '.' && path[1] == '.';
}

bool LibraryLocator::isSlash(char value)
{
	return value == '/' || value == '\\';
}

char LibraryLocator::toLower(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value - 'A' + 'a');

	return value;
}

bool LibraryLocator::hasExtension(const char *path)
{
	if (StringEmpty(path))
		return false;

	const char *lastSlash = nullptr;
	const char *lastDot = nullptr;

	for (const char *cursor = path; *cursor != '\0'; ++cursor)
	{
		if (isSlash(*cursor))
			lastSlash = cursor;
		else if (*cursor == '.')
			lastDot = cursor;
	}

	return lastDot != nullptr && (lastSlash == nullptr || lastDot > lastSlash);
}

bool LibraryLocator::appendDefaultExtension(
	char *output,
	size_t outputSize,
	const char *extension)
{
	if (StringEmpty(extension))
		return true;

	const size_t outputLength = strlen(output);
	const size_t extensionLength = strlen(extension);

	if (outputLength + extensionLength + 1 > outputSize)
		return false;

	memcpy(output + outputLength, extension, extensionLength + 1);
	return true;
}

}
}
