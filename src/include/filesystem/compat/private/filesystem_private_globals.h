#ifndef XASH_FILESYSTEM_PRIVATE_GLOBALS_H
#define XASH_FILESYSTEM_PRIVATE_GLOBALS_H

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern fs_globals_t   FI;
extern searchpath_t  *fs_writepath;
extern poolhandle_t   fs_mempool;
extern fs_interface_t g_engfuncs;
extern char           fs_rootdir[MAX_SYSPATH];
extern const fs_api_t g_api;

#define GI FI.GameInfo

#ifdef __cplusplus
}
#endif

#endif
