#ifndef XASH_FILESYSTEM_LIBRARY_LOCATOR_ADAPTER_H
#define XASH_FILESYSTEM_LIBRARY_LOCATOR_ADAPTER_H

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

qboolean FS_LibraryLocator_NormalizeShortPath(
	const char *dllname,
	char *output,
	size_t outputSize );
qboolean FS_LibraryLocator_ShouldCheckEncryption( const char *shortPath );

#ifdef __cplusplus
}
#endif

#endif
