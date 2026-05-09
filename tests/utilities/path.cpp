#include <stdlib.h>
#include <string.h>

#include "utilities/path.hpp"

using namespace xash::utilities;

static bool ExpectString(const char *actual, const char *expected)
{
	return strcmp(actual, expected) == 0;
}

static bool TestFileBaseAndExtension()
{
	char out[64];

	FileBase("zxc/asd/qvert.lkjefgkljh", out, sizeof(out));
	if (!ExpectString(out, "qvert"))
		return false;

	FileBase("pak0.pk3/texture", out, sizeof(out));
	if (!ExpectString(out, "texture"))
		return false;

	FileBase("blep/.nomedia", out, sizeof(out));
	if (!ExpectString(out, ""))
		return false;

	FileBase("qwertyuiop", out, 3);
	if (!ExpectString(out, "qw"))
		return false;

	if (!ExpectString(FileExtension("path/to/file.wad"), "wad"))
		return false;

	if (!ExpectString(FileExtension("inside.wad/AAATRIGGER"), ""))
		return false;

	return ExpectString(FileExtension("c:/games/gamedir/liblist.cum"), "cum");
}

static bool TestPathExtraction()
{
	char out[64];

	if (!ExpectString(FileWithoutPath("c:/games/valve/pak0.pak"), "pak0.pak"))
		return false;

	if (!ExpectString(FileWithoutPath("drive:file.txt"), "file.txt"))
		return false;

	ExtractFilePath("keep/the/original/func/behavior/", out);
	if (!ExpectString(out, "keep/the/original/func"))
		return false;

	ExtractFilePath("backslashes\\are\\annoying\\af", out);
	return ExpectString(out, "backslashes\\are\\annoying");
}

static bool TestExtensionMutation()
{
	char path[64];

	strcpy(path, "dir/file.ext");
	StripExtension(path);
	if (!ExpectString(path, "dir/file"))
		return false;

	strcpy(path, "dir.ext/file");
	StripExtension(path);
	if (!ExpectString(path, "dir.ext/file"))
		return false;

	strcpy(path, "dir/file");
	DefaultExtension(path, ".cfg", sizeof(path));
	if (!ExpectString(path, "dir/file.cfg"))
		return false;

	strcpy(path, "dir/file.ext");
	ReplaceExtension(path, ".cfg", sizeof(path));
	if (!ExpectString(path, "dir/file.cfg"))
		return false;

	strcpy(path, ".nomedia");
	ReplaceExtension(path, ".cfg", sizeof(path));
	return ExpectString(path, ".nomedia.cfg");
}

static bool TestSlashFixes()
{
	char path[64];

	strcpy(path, "dir\\");
	PathSlashFix(path);
	if (!ExpectString(path, "dir/"))
		return false;

	strcpy(path, "dir");
	PathSlashFix(path);
	if (!ExpectString(path, "dir/"))
		return false;

	strcpy(path, "path\\\\with//mixed\\\\slashes");
	NormalizeSlashes(path);
	return ExpectString(path, "path/with/mixed/slashes");
}

int main()
{
	if (!TestFileBaseAndExtension() ||
		!TestPathExtraction() ||
		!TestExtensionMutation() ||
		!TestSlashFixes())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
