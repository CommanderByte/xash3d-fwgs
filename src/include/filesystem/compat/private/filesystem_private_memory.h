#ifndef XASH_FILESYSTEM_PRIVATE_MEMORY_H
#define XASH_FILESYSTEM_PRIVATE_MEMORY_H

#include "filesystem/compat/private/filesystem_private_globals.h"

#ifdef __cplusplus
extern "C"
{
#endif

void _Mem_Free( void *data, const char *filename, int fileline );
void *_Mem_Alloc( poolhandle_t poolptr, size_t size, qboolean clear,
	const char *filename, int fileline )
	ALLOC_CHECK( 2 ) MALLOC_LIKE( _Mem_Free, 1 ) WARN_UNUSED_RESULT;

#define Mem_Malloc( pool, size ) _Mem_Alloc( pool, size, false, __FILE__, __LINE__ )
#define Mem_Calloc( pool, size ) _Mem_Alloc( pool, size, true, __FILE__, __LINE__ )
#define Mem_Realloc( pool, ptr, size ) g_engfuncs._Mem_Realloc( pool, ptr, size, true, __FILE__, __LINE__ )
#define Mem_Free( mem ) _Mem_Free( mem, __FILE__, __LINE__ )
#define Mem_AllocPool( name ) g_engfuncs._Mem_AllocPool( name, __FILE__, __LINE__ )
#define Mem_FreePool( pool ) g_engfuncs._Mem_FreePool( pool, __FILE__, __LINE__ )

#define Con_Printf  ( *g_engfuncs._Con_Printf )
#define Con_DPrintf ( *g_engfuncs._Con_DPrintf )
#define Con_Reportf ( *g_engfuncs._Con_Reportf )
#define Sys_Error   ( *g_engfuncs._Sys_Error )
#define Sys_GetNativeObject ( *g_engfuncs._Sys_GetNativeObject )

#ifdef __cplusplus
}
#endif

#endif
