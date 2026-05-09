#include "filesystem/compat/library_locator_adapter.h"

#include "filesystem/compat/private/filesystem_private_globals.h"

#include "filesystem/library_locator.hpp"

using xash::filesystem::LibraryLocator;
using xash::filesystem::LibraryShortPathConfig;

qboolean FS_LibraryLocator_NormalizeShortPath(
	const char *dllname,
	char *output,
	size_t outputSize )
{
	LibraryShortPathConfig config = {};

	config.requestedName = dllname;
	config.gameFolder = GI->gamefolder;
	config.fallbackGameFolder = "valve";
	config.defaultExtension = "." OS_LIB_EXT;

	return LibraryLocator::normalizeShortPath( config, output, outputSize )
		? true
		: false;
}

qboolean FS_LibraryLocator_ShouldCheckEncryption( const char *shortPath )
{
	return LibraryLocator::shouldCheckEncryption( shortPath, "dll" )
		? true
		: false;
}
