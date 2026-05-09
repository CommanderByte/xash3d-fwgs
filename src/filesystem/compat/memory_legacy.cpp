#include "filesystem/compat/private/filesystem_private_memory.h"

extern "C" {

void _Mem_Free( void *data, const char *filename, int fileline )
{
	g_engfuncs._Mem_Free( data, filename, fileline );
}

void *_Mem_Alloc( poolhandle_t poolptr, size_t size, qboolean clear,
	const char *filename, int fileline )
{
	return g_engfuncs._Mem_Alloc( poolptr, size, clear, filename, fileline );
}

}
