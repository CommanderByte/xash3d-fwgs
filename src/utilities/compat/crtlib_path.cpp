#include "utilities/path.hpp"

extern "C"
{
#include "crtlib.h"
}

extern "C" void COM_FileBase(const char *in, char *out, size_t size)
{
	xash::utilities::FileBase(in, out, size);
}

extern "C" const char *COM_FileExtension(const char *in)
{
	return xash::utilities::FileExtension(in);
}

extern "C" const char *COM_FileWithoutPath(const char *in)
{
	return xash::utilities::FileWithoutPath(in);
}

extern "C" void COM_ExtractFilePath(const char *path, char *dest)
{
	xash::utilities::ExtractFilePath(path, dest);
}

extern "C" void COM_StripExtension(char *path)
{
	xash::utilities::StripExtension(path);
}

extern "C" void COM_DefaultExtension(char *path, const char *extension, size_t size)
{
	xash::utilities::DefaultExtension(path, extension, size);
}

extern "C" void COM_ReplaceExtension(char *path, const char *extension, size_t size)
{
	xash::utilities::ReplaceExtension(path, extension, size);
}

extern "C" void COM_PathSlashFix(char *path)
{
	xash::utilities::PathSlashFix(path);
}
