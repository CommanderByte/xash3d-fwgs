#include "client_command_dispatch_adapter.h"

#include "engine/server/client_command_dispatch.hpp"

namespace
{

sv_client_command_route_t ToLegacyRoute(xash::engine::server::ClientCommandRoute route)
{
	using xash::engine::server::ClientCommandRoute;

	switch (route)
	{
	case ClientCommandRoute::Ignore:
		return SV_CLIENT_COMMAND_IGNORE;
	case ClientCommandRoute::Builtin:
		return SV_CLIENT_COMMAND_BUILTIN;
	case ClientCommandRoute::EntTools:
		return SV_CLIENT_COMMAND_ENTTOOLS;
	case ClientCommandRoute::GameDll:
		return SV_CLIENT_COMMAND_GAME_DLL;
	case ClientCommandRoute::FullUpdate:
		return SV_CLIENT_COMMAND_FULLUPDATE;
	}

	return SV_CLIENT_COMMAND_IGNORE;
}

}

extern "C" sv_client_command_decision_t SV_ClientCommandDispatch_Classify(
	const char *command_name,
	int server_active,
	int client_spawned,
	int enttools_enabled,
	int server_background,
	int fullupdate_throttled)
{
	xash::engine::server::ClientCommandContext context = {};
	context.commandName = command_name;
	context.serverActive = server_active != 0;
	context.clientSpawned = client_spawned != 0;
	context.entToolsEnabled = enttools_enabled != 0;
	context.serverBackground = server_background != 0;
	context.fullUpdateThrottled = fullupdate_throttled != 0;

	const xash::engine::server::ClientCommandDecision modern =
		xash::engine::server::ClassifyClientCommand(context);

	sv_client_command_decision_t legacy = {};
	legacy.route = ToLegacyRoute(modern.route);
	legacy.command_index = modern.commandIndex;
	return legacy;
}

extern "C" int SV_ClientCommandDispatch_BuiltinCount(void)
{
	return xash::engine::server::ClientBuiltinCommandCount();
}

extern "C" const char *SV_ClientCommandDispatch_BuiltinName(int index)
{
	return xash::engine::server::ClientBuiltinCommandName(index);
}

extern "C" int SV_ClientCommandDispatch_EntToolsCount(void)
{
	return xash::engine::server::ClientEntToolsCommandCount();
}

extern "C" const char *SV_ClientCommandDispatch_EntToolsName(int index)
{
	return xash::engine::server::ClientEntToolsCommandName(index);
}
