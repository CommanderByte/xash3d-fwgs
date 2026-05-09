/*
base_cmd_adapter.cpp - private bridge from legacy BaseCmd C API to C++ registry
*/

#include "base_cmd_adapter.h"

#include "engine/commands/base_command_registry.hpp"

namespace
{

xash::engine::commands::BaseCommandRegistry g_baseCommandRegistry;

xash::engine::commands::BaseCommandType ToModernType( base_command_type_e type )
{
	using xash::engine::commands::BaseCommandType;

	switch( type )
	{
	case HM_CVAR:
		return BaseCommandType::Cvar;
	case HM_CMD:
		return BaseCommandType::Command;
	case HM_CMDALIAS:
		return BaseCommandType::Alias;
	default:
		return BaseCommandType::DontCare;
	}
}

}

extern "C" void BaseCmdAdapter_Init( void )
{
	g_baseCommandRegistry.clear();
}

extern "C" void BaseCmdAdapter_Shutdown( void )
{
	g_baseCommandRegistry.clear();
}

extern "C" base_command_t *BaseCmdAdapter_Find( base_command_type_e type, const char *name )
{
	const xash::engine::commands::BaseCommandType modernType = ToModernType( type );

	if( modernType == xash::engine::commands::BaseCommandType::DontCare )
		return nullptr;

	return static_cast<base_command_t *>(g_baseCommandRegistry.find( modernType, name ));
}

extern "C" void BaseCmdAdapter_FindAll( const char *name, cmd_t **cmd, cmdalias_t **alias, convar_t **cvar )
{
	const xash::engine::commands::BaseCommandMatches matches = g_baseCommandRegistry.findAll( name );

	*cmd = static_cast<cmd_t *>(matches.command);
	*alias = static_cast<cmdalias_t *>(matches.alias);
	*cvar = static_cast<convar_t *>(matches.cvar);
}

extern "C" qboolean BaseCmdAdapter_Insert( base_command_type_e type, base_command_t *basecmd, const char *name )
{
	const xash::engine::commands::BaseCommandType modernType = ToModernType( type );

	if( modernType == xash::engine::commands::BaseCommandType::DontCare )
		return false;

	return g_baseCommandRegistry.insert( modernType, basecmd, name ) ? true : false;
}

extern "C" qboolean BaseCmdAdapter_Remove( base_command_type_e type, const char *name )
{
	const xash::engine::commands::BaseCommandType modernType = ToModernType( type );

	if( modernType == xash::engine::commands::BaseCommandType::DontCare )
		return false;

	return g_baseCommandRegistry.remove( modernType, name ) ? true : false;
}

extern "C" basecmd_adapter_stats_t BaseCmdAdapter_Stats( void )
{
	const xash::engine::commands::BaseCommandBucketStats modernStats = g_baseCommandRegistry.stats();

	basecmd_adapter_stats_t stats;
	stats.min_depth = modernStats.minDepth;
	stats.max_depth = modernStats.maxDepth;
	stats.empty_buckets = modernStats.emptyBuckets;
	return stats;
}
