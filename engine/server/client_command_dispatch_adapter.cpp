#include "client_command_dispatch_adapter.h"

#include "client_adapter_shared.hpp"
#include "engine/server/client/client_command_dispatch.hpp"

using xash::engine::server::ClientCommandRoute;
using xash::engine::server::adapter::client::FromLegacyBool;
using xash::engine::server::adapter::client::ToLegacyEnum;

static_assert(SV_CLIENT_COMMAND_IGNORE ==
	static_cast<int>(ClientCommandRoute::Ignore),
	"ClientCommandRoute::Ignore value changed");
static_assert(SV_CLIENT_COMMAND_BUILTIN ==
	static_cast<int>(ClientCommandRoute::Builtin),
	"ClientCommandRoute::Builtin value changed");
static_assert(SV_CLIENT_COMMAND_ENTTOOLS ==
	static_cast<int>(ClientCommandRoute::EntTools),
	"ClientCommandRoute::EntTools value changed");
static_assert(SV_CLIENT_COMMAND_GAME_DLL ==
	static_cast<int>(ClientCommandRoute::GameDll),
	"ClientCommandRoute::GameDll value changed");
static_assert(SV_CLIENT_COMMAND_FULLUPDATE ==
	static_cast<int>(ClientCommandRoute::FullUpdate),
	"ClientCommandRoute::FullUpdate value changed");

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
	context.serverActive = FromLegacyBool(server_active);
	context.clientSpawned = FromLegacyBool(client_spawned);
	context.entToolsEnabled = FromLegacyBool(enttools_enabled);
	context.serverBackground = FromLegacyBool(server_background);
	context.fullUpdateThrottled = FromLegacyBool(fullupdate_throttled);

	const xash::engine::server::ClientCommandDecision modern =
		xash::engine::server::ClassifyClientCommand(context);

	sv_client_command_decision_t legacy = {};
	legacy.route = static_cast<sv_client_command_route_t>(
		ToLegacyEnum(modern.route));
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
