/*
base_cmd.c - command & cvar hashmap. Insipred by Doom III
Copyright (C) 2016 a1batross

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "base_cmd.h"
#include "base_cmd_adapter.h"

/*
============
BaseCmd_Find

Find base command in hashmap
============
*/
base_command_t *BaseCmd_Find( base_command_type_e type, const char *name )
{
	return BaseCmdAdapter_Find( type, name );
}

/*
============
BaseCmd_Find

Find every type of base command and write into arguments
============
*/
void BaseCmd_FindAll( const char *name, cmd_t **cmd, cmdalias_t **alias, convar_t **cvar )
{
	BaseCmdAdapter_FindAll( name, cmd, alias, cvar );
}

/*
============
BaseCmd_Insert

Add new typed base command to hashmap
============
*/
void BaseCmd_Insert( base_command_type_e type, base_command_t *basecmd, const char *name )
{
	if( !BaseCmdAdapter_Insert( type, basecmd, name ))
		Con_Reportf( S_ERROR "%s: Couldn't insert %s in buckets\n", __func__, name );
}

/*
============
BaseCmd_Remove

Remove base command from hashmap
============
*/
void BaseCmd_Remove( base_command_type_e type, const char *name )
{
	if( !BaseCmdAdapter_Remove( type, name ))
		Con_Reportf( S_ERROR "%s: Couldn't find %s in buckets\n", __func__, name );
}

/*
============
BaseCmd_Init

initialize base command hashmap system
============
*/
void BaseCmd_Init( void )
{
	BaseCmdAdapter_Init();
}

void BaseCmd_Shutdown( void )
{
	BaseCmdAdapter_Shutdown();
}

/*
============
BaseCmd_Stats_f

============
*/
void BaseCmd_Stats_f( void )
{
	basecmd_adapter_stats_t stats = BaseCmdAdapter_Stats();

	Con_Printf( "min length: %zu, max length: %zu, empty: %zu\n", stats.min_depth, stats.max_depth, stats.empty_buckets );
}

struct basecmd_test_stats_s
{
	qboolean valid;
	int lookups;
};

static void BaseCmd_CheckCvars( const char *key, const char *value, const void *unused, void *ptr )
{
	struct basecmd_test_stats_s *stats = ptr;

	stats->lookups++;
	if( !BaseCmd_Find( HM_CVAR, key ))
	{
		Con_Printf( "Cvar %s is missing in basecmd\n", key );
		stats->valid = false;
	}
}

/*
============
BaseCmd_Stats_f

testing order matches cbuf execute
============
*/
void BaseCmd_Test_f( void )
{
	struct basecmd_test_stats_s stats =
	{
		.valid = true,
	};

	double start = Platform_DoubleTime() * 1000;

	for( int i = 0; i < 1000; i++ )
	{
		cmdalias_t *a;
		void *cmd;

		// Cmd_LookupCmds don't allows to check alias, so just iterate
		for( a = Cmd_AliasGetList(); a; a = a->next, stats.lookups++ )
		{
			if( !BaseCmd_Find( HM_CMDALIAS, a->name ))
			{
				Con_Printf( "Alias %s is missing in basecmd\n", a->name );
				stats.valid = false;
			}
		}

		for( cmd = Cmd_GetFirstFunctionHandle(); cmd;
			 cmd = Cmd_GetNextFunctionHandle( cmd ), stats.lookups++ )
		{
			if( !BaseCmd_Find( HM_CMD, Cmd_GetName( cmd )))
			{
				Con_Printf( "Command %s is missing in basecmd\n", Cmd_GetName( cmd ));
				stats.valid = false;
			}
		}

		Cvar_LookupVars( 0, NULL, &stats.valid, (setpair_t)BaseCmd_CheckCvars );
	}

	double end = Platform_DoubleTime() * 1000;
	double dt = end - start;

	if( !stats.valid )
		Con_Printf( "BaseCmd is valid\n" );

	Con_Printf( "Test took %.3f ms, %d lookups, %.3f us/lookup\n", dt, stats.lookups, dt / stats.lookups * 1000 );

	BaseCmd_Stats_f();
}
