#include "utilities/path.hpp"

#include <string.h>

namespace xash
{
namespace utilities
{

namespace
{

size_t StringLength(const char *value)
{
	return value ? strlen(value) : 0;
}

size_t CopyString(char *dst, const char *src, size_t size)
{
	if (!dst || !src || size == 0)
		return 0;

	const size_t len = strlen(src);

	if (len >= size)
	{
		memcpy(dst, src, size);
		dst[size - 1] = '\0';
	}
	else
	{
		memcpy(dst, src, len + 1);
	}

	return len;
}

const char *FindLastChar(const char *value, char needle)
{
	return value ? strrchr(value, needle) : nullptr;
}

const char *FindAnyChar(const char *value, const char *needles)
{
	return value ? strpbrk(value, needles) : nullptr;
}

bool IsSeparator(char value)
{
	return value == '/' || value == '\\' || value == ':';
}

}

void FileBase(const char *in, char *out, size_t size)
{
	const char *dot = nullptr;
	const char *slash;
	const char *cursor;
	size_t len;

	if (!out)
		return;

	if (!in || in[0] == '\0' || size <= 1)
	{
		out[0] = '\0';
		return;
	}

	slash = in;
	for (cursor = in; *cursor; ++cursor)
	{
		if (*cursor == '/' || *cursor == '\\')
			slash = cursor + 1;

		if (*cursor == '.')
			dot = cursor;
	}

	if (dot == nullptr || dot < slash)
		dot = cursor;

	len = static_cast<size_t>(dot - slash);
	if (len > size - 1)
		len = size - 1;

	memcpy(out, slash, len);
	out[len] = '\0';
}

const char *FileExtension(const char *in)
{
	const char *dot = FindLastChar(in, '.');

	if (dot == nullptr)
		return "";

	if (FindAnyChar(dot + 1, "\\/:"))
		return "";

	return dot + 1;
}

const char *FileWithoutPath(const char *in)
{
	const char *separator = FindLastChar(in, '/');
	const char *backslash = FindLastChar(in, '\\');
	const char *colon = FindLastChar(in, ':');

	if (!in)
		return "";

	if (!separator || separator < backslash)
		separator = backslash;

	if (!separator || separator < colon)
		separator = colon;

	return separator ? separator + 1 : in;
}

void ExtractFilePath(const char *path, char *dest)
{
	if (!dest)
		return;

	if (!path || path[0] == '\0')
	{
		dest[0] = '\0';
		return;
	}

	const char *src = path + strlen(path) - 1;

	while (src > path && !(*(src - 1) == '\\' || *(src - 1) == '/'))
		--src;

	if (src > path)
	{
		memcpy(dest, path, static_cast<size_t>(src - path));
		dest[src - path - 1] = '\0';
	}
	else
	{
		dest[0] = '\0';
	}
}

void StripExtension(char *path)
{
	size_t length = StringLength(path);

	if (!path)
		return;

	if (length > 0)
		--length;

	while (length > 0 && path[length] != '.')
	{
		--length;
		if (IsSeparator(path[length]))
			return;
	}

	if (length)
		path[length] = '\0';
}

void DefaultExtension(char *path, const char *extension, size_t size)
{
	if (!path || !extension || size == 0)
		return;

	const size_t len = strlen(path);

	if (len > 0)
	{
		const char *src = path + len - 1;

		while (*src != '/' && src != path)
		{
			if (*src == '.')
				return;
			--src;
		}
	}

	if (len < size)
		CopyString(&path[len], extension, size - len);
}

void ReplaceExtension(char *path, const char *extension, size_t size)
{
	StripExtension(path);
	DefaultExtension(path, extension, size);
}

void PathSlashFix(char *path)
{
	if (!path)
		return;

	const size_t len = strlen(path);

	if (len == 0)
		return;

	if (path[len - 1] == '\\')
	{
		path[len - 1] = '/';
	}
	else if (path[len - 1] != '/')
	{
		path[len] = '/';
		path[len + 1] = '\0';
	}
}

void NormalizeSlashes(char *path)
{
	if (!path)
		return;

	char *cursor = path;
	while ((cursor = strchr(cursor, '\\')) != nullptr)
		*cursor = '/';

	int read = 0;
	int write = 0;
	while (path[read])
	{
		if (path[read] == '/' && path[read + 1] == '/')
		{
			++read;
			continue;
		}

		path[write++] = path[read++];
	}
	path[write] = '\0';
}

}
}
