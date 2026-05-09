/*
base_cmd_adapter.h - private bridge from legacy BaseCmd C API to C++ registry
*/
#ifndef BASE_CMD_ADAPTER_H
#define BASE_CMD_ADAPTER_H

#include "base_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct basecmd_adapter_stats_s
{
	size_t min_depth;
	size_t max_depth;
	size_t empty_buckets;
} basecmd_adapter_stats_t;

void BaseCmdAdapter_Init( void );
void BaseCmdAdapter_Shutdown( void );
base_command_t *BaseCmdAdapter_Find( base_command_type_e type, const char *name );
void BaseCmdAdapter_FindAll( const char *name, cmd_t **cmd, cmdalias_t **alias, convar_t **cvar );
qboolean BaseCmdAdapter_Insert( base_command_type_e type, base_command_t *basecmd, const char *name );
qboolean BaseCmdAdapter_Remove( base_command_type_e type, const char *name );
basecmd_adapter_stats_t BaseCmdAdapter_Stats( void );

#ifdef __cplusplus
}
#endif

#endif // BASE_CMD_ADAPTER_H
