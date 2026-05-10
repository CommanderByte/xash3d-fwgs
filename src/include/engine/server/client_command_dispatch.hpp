#ifndef XASH_ENGINE_SERVER_CLIENT_COMMAND_DISPATCH_HPP
#define XASH_ENGINE_SERVER_CLIENT_COMMAND_DISPATCH_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class ClientCommandRoute
{
	Ignore,
	Builtin,
	EntTools,
	GameDll,
	FullUpdate,
};

struct ClientCommandContext
{
	const char *commandName;
	bool serverActive;
	bool clientSpawned;
	bool entToolsEnabled;
	bool serverBackground;
	bool fullUpdateThrottled;
};

struct ClientCommandDecision
{
	ClientCommandRoute route;
	int commandIndex;
};

int ClientBuiltinCommandCount();
const char *ClientBuiltinCommandName(int index);

int ClientEntToolsCommandCount();
const char *ClientEntToolsCommandName(int index);

ClientCommandDecision ClassifyClientCommand(const ClientCommandContext &context);

}
}
}

#endif
