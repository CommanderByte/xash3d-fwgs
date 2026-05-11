#include "engine/server/client/client_command_dispatch.hpp"

#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr const char *kBuiltinCommands[] =
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

constexpr const char *kEntToolsCommands[] =
{
	"ent_create",
	"ent_fire",
	"ent_getvars",
	"ent_info",
	"ent_list",
};

constexpr const char *kFullUpdateCommand = "fullupdate";

const char *SafeString(const char *value)
{
	if (!value)
		return "";

	return value;
}

bool Equals(const char *lhs, const char *rhs)
{
	return std::strcmp(SafeString(lhs), rhs) == 0;
}

int FindCommand(const char *name, const char *const *commands, int count)
{
	for (int i = 0; i < count; ++i)
	{
		if (Equals(name, commands[i]))
			return i;
	}

	return -1;
}

ClientCommandDecision MakeDecision(ClientCommandRoute route, int commandIndex = -1)
{
	ClientCommandDecision decision = {};
	decision.route = route;
	decision.commandIndex = commandIndex;
	return decision;
}

}

int ClientBuiltinCommandCount()
{
	return static_cast<int>(sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0]));
}

const char *ClientBuiltinCommandName(int index)
{
	if (index < 0 || index >= ClientBuiltinCommandCount())
		return nullptr;

	return kBuiltinCommands[index];
}

int ClientEntToolsCommandCount()
{
	return static_cast<int>(sizeof(kEntToolsCommands) / sizeof(kEntToolsCommands[0]));
}

const char *ClientEntToolsCommandName(int index)
{
	if (index < 0 || index >= ClientEntToolsCommandCount())
		return nullptr;

	return kEntToolsCommands[index];
}

ClientCommandDecision ClassifyClientCommand(const ClientCommandContext &context)
{
	const int builtin = FindCommand(
		context.commandName,
		kBuiltinCommands,
		ClientBuiltinCommandCount());

	if (builtin >= 0)
		return MakeDecision(ClientCommandRoute::Builtin, builtin);

	if (!context.serverActive)
		return MakeDecision(ClientCommandRoute::Ignore);

	if (context.clientSpawned && context.entToolsEnabled && !context.serverBackground)
	{
		const int entTools = FindCommand(
			context.commandName,
			kEntToolsCommands,
			ClientEntToolsCommandCount());

		if (entTools >= 0)
			return MakeDecision(ClientCommandRoute::EntTools, entTools);
	}

	if (Equals(context.commandName, kFullUpdateCommand))
	{
		if (context.fullUpdateThrottled)
			return MakeDecision(ClientCommandRoute::Ignore);

		return MakeDecision(ClientCommandRoute::FullUpdate);
	}

	return MakeDecision(ClientCommandRoute::GameDll);
}

}
}
}
