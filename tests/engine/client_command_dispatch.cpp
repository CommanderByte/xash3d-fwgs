#include <cstdlib>
#include <cstring>

#include "engine/server/client/client_command_dispatch.hpp"

using xash::engine::server::ClassifyClientCommand;
using xash::engine::server::ClientBuiltinCommandCount;
using xash::engine::server::ClientBuiltinCommandName;
using xash::engine::server::ClientCommandContext;
using xash::engine::server::ClientCommandDecision;
using xash::engine::server::ClientCommandRoute;
using xash::engine::server::ClientEntToolsCommandCount;
using xash::engine::server::ClientEntToolsCommandName;

static ClientCommandDecision Classify(
	const char *name,
	bool serverActive = true,
	bool clientSpawned = true,
	bool entToolsEnabled = true,
	bool serverBackground = false,
	bool fullUpdateThrottled = false)
{
	ClientCommandContext context = {};
	context.commandName = name;
	context.serverActive = serverActive;
	context.clientSpawned = clientSpawned;
	context.entToolsEnabled = entToolsEnabled;
	context.serverBackground = serverBackground;
	context.fullUpdateThrottled = fullUpdateThrottled;
	return ClassifyClientCommand(context);
}

static bool IsRoute(
	const char *name,
	ClientCommandRoute route,
	int index = -1,
	bool serverActive = true,
	bool clientSpawned = true,
	bool entToolsEnabled = true,
	bool serverBackground = false,
	bool fullUpdateThrottled = false)
{
	const ClientCommandDecision decision = Classify(
		name,
		serverActive,
		clientSpawned,
		entToolsEnabled,
		serverBackground,
		fullUpdateThrottled);

	return decision.route == route && decision.commandIndex == index;
}

static bool TestBuiltinTable()
{
	static const char *expected[] =
	{
		"_sv_build_info",
		"begin",
		"disconnect",
		"dlfile",
		"god",
		"info",
		"kill",
		"new",
		"noclip",
		"notarget",
		"pause",
		"sendres",
		"setinfo",
		"spawn",
		"status",
	};

	if (ClientBuiltinCommandCount() != static_cast<int>(sizeof(expected) / sizeof(expected[0])))
		return false;

	for (int i = 0; i < ClientBuiltinCommandCount(); ++i)
	{
		if (std::strcmp(ClientBuiltinCommandName(i), expected[i]) != 0)
			return false;

		if (!IsRoute(expected[i], ClientCommandRoute::Builtin, i, false, false, false, true, true))
			return false;
	}

	return ClientBuiltinCommandName(-1) == nullptr &&
		ClientBuiltinCommandName(ClientBuiltinCommandCount()) == nullptr;
}

static bool TestEntToolsTableAndGates()
{
	static const char *expected[] =
	{
		"ent_create",
		"ent_fire",
		"ent_getvars",
		"ent_info",
		"ent_list",
	};

	if (ClientEntToolsCommandCount() != static_cast<int>(sizeof(expected) / sizeof(expected[0])))
		return false;

	for (int i = 0; i < ClientEntToolsCommandCount(); ++i)
	{
		if (std::strcmp(ClientEntToolsCommandName(i), expected[i]) != 0)
			return false;

		if (!IsRoute(expected[i], ClientCommandRoute::EntTools, i))
			return false;

		if (!IsRoute(expected[i], ClientCommandRoute::GameDll, -1, true, false, true, false, false))
			return false;

		if (!IsRoute(expected[i], ClientCommandRoute::GameDll, -1, true, true, false, false, false))
			return false;

		if (!IsRoute(expected[i], ClientCommandRoute::GameDll, -1, true, true, true, true, false))
			return false;
	}

	return ClientEntToolsCommandName(-1) == nullptr &&
		ClientEntToolsCommandName(ClientEntToolsCommandCount()) == nullptr;
}

static bool TestFullUpdateAndFallback()
{
	return IsRoute("fullupdate", ClientCommandRoute::FullUpdate) &&
		IsRoute("fullupdate", ClientCommandRoute::Ignore, -1, true, true, true, false, true) &&
		IsRoute("fullupdate", ClientCommandRoute::Ignore, -1, false, true, true, false, false) &&
		IsRoute("unknown", ClientCommandRoute::GameDll) &&
		IsRoute("unknown", ClientCommandRoute::Ignore, -1, false, true, true, false, false) &&
		IsRoute("", ClientCommandRoute::GameDll) &&
		IsRoute(nullptr, ClientCommandRoute::GameDll);
}

static bool TestCaseSensitivity()
{
	return IsRoute("Begin", ClientCommandRoute::GameDll) &&
		IsRoute("ENT_FIRE", ClientCommandRoute::GameDll) &&
		IsRoute("FullUpdate", ClientCommandRoute::GameDll);
}

int main()
{
	if (!TestBuiltinTable() ||
		!TestEntToolsTableAndGates() ||
		!TestFullUpdateAndFallback() ||
		!TestCaseSensitivity())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
