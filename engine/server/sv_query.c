/*
sv_query.c - Source-engine like server querying
Copyright (C) 2023 jeefo

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
#include "server.h"
#include "client_policy_adapter.h"
#include "source_query_adapter.h"

/*
==================
SV_SourceQuery_Details
==================
*/
static void SV_SourceQuery_Details( netadr_t from )
{
	char answer[2048];
	sv_source_query_details_t details;
	int bot_count, client_count;
	int total;

	SV_GetPlayerCount( &client_count, &bot_count );
	client_count += bot_count; // bots are counted as players in this reply

	memset( &details, 0, sizeof( details ));
	details.protocol_version = PROTOCOL_VERSION;
	details.hostname = hostname.string;
	details.map_name = sv.name;
	details.game_folder = GI->gamefolder;
	details.game_description = svgame.dllFuncs.pfnGetGameDescription( );
	details.app_id = 0;
	details.player_count = client_count;
	details.max_players = svs.maxclients;
	details.bot_count = bot_count;
	details.server_type = Host_IsDedicated( ) ? 'd' : 'l';
	details.platform = SV_SourceQueryAdapter_PlatformCode( );
	details.password_protected = SV_HavePassword( );
	details.secure = GI->secure;
	details.version = XASH_VERSION;

	total = SV_SourceQueryAdapter_BuildDetails( answer, sizeof( answer ), &details );
	if( total > 0 )
		NET_SendPacket( NS_SERVER, total, answer, from );
}

/*
==================
SV_SourceQuery_Rules
==================
*/
static void SV_SourceQuery_Rules( netadr_t from )
{
	const cvar_t *cvar;
	char answer[MAX_PRINT_MSG - 4];
	int total;
	uint cvar_count = 0;

	total = SV_SourceQueryAdapter_BeginRules( answer, sizeof( answer ));
	if( total <= 0 )
		return;

	for( cvar = Cvar_GetList( ); cvar; cvar = cvar->next )
	{
		int next;

		if( !FBitSet( cvar->flags, FCVAR_SERVER ))
			continue;

		next = SV_SourceQueryAdapter_AppendRule(
			answer,
			sizeof( answer ),
			total,
			cvar->name,
			cvar->string,
			FBitSet( cvar->flags, FCVAR_PROTECTED ));

		if( next <= 0 )
			break;

		total = next;
		cvar_count++;
	}

	if( cvar_count != 0 )
	{
		total = SV_SourceQueryAdapter_FinishRules( answer, sizeof( answer ), total, cvar_count );
		if( total > 0 )
			NET_SendPacket( NS_SERVER, total, answer, from );
	}
}

/*
==================
SV_SourceQuery_Players
==================
*/
static void SV_SourceQuery_Players( netadr_t from )
{
	char answer[MAX_PRINT_MSG - 4];
	int i, count = 0;
	int total;

	// respect players privacy
	if( !SV_SourceQueryAdapter_AllowsPlayerList( sv_expose_player_list.value != 0.0f, SV_HavePassword( )))
		return;

	total = SV_SourceQueryAdapter_BeginPlayers( answer, sizeof( answer ));
	if( total <= 0 )
		return;

	for( i = 0; i < svs.maxclients; i++ )
	{
		const sv_client_t *cl = &svs.clients[i];
		float duration;
		int next;

		if( cl->state < cs_connected )
			continue;

		if( !SV_ClientPolicy_ShouldAppearInHumanQueries( cl->flags ))
			duration = -1.0f;
		else duration = host.realtime - cl->connection_started;

		next = SV_SourceQueryAdapter_AppendPlayer(
			answer,
			sizeof( answer ),
			total,
			count,
			cl->name,
			cl->edict->v.frags,
			duration );

		if( next <= 0 )
			break;

		total = next;
		count++;
	}

	if( count != 0 )
	{
		total = SV_SourceQueryAdapter_FinishPlayers( answer, sizeof( answer ), total, count );
		if( total > 0 )
			NET_SendPacket( NS_SERVER, total, answer, from );
	}
}

/*
==================
SV_SourceQuery_HandleConnnectionlessPacket
==================
*/
void SV_SourceQuery_HandleConnnectionlessPacket( const char *c, netadr_t from )
{
	if( !Q_strcmp( c, A2S_GOLDSRC_INFO ))
	{
		SV_SourceQuery_Details( from );
	}
	else switch( c[0] )
	{
	case A2S_GOLDSRC_RULES:
		SV_SourceQuery_Rules( from );
		break;
	case A2S_GOLDSRC_PLAYERS:
		SV_SourceQuery_Players( from );
		break;
	}
}
