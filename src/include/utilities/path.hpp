#ifndef XASH_UTILITIES_PATH_HPP
#define XASH_UTILITIES_PATH_HPP

#include <stddef.h>

namespace xash
{
namespace utilities
{

void FileBase(const char *in, char *out, size_t size);
const char *FileExtension(const char *in);
const char *FileWithoutPath(const char *in);
void ExtractFilePath(const char *path, char *dest);
void StripExtension(char *path);
void DefaultExtension(char *path, const char *extension, size_t size);
void ReplaceExtension(char *path, const char *extension, size_t size);
void PathSlashFix(char *path);
void NormalizeSlashes(char *path);

}
}

#endif
