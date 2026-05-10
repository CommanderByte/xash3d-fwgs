#ifndef XASH_ENGINE_SERVER_GAME_DLL_CLIENT_INFO_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_CLIENT_INFO_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

enum class GameDllInfoBufferRoute
{
	LocalInfo,
	ServerInfo,
	ClientUserinfo,
	EmptyString,
};

enum class GameDllSetValueAction
{
	SetLocalInfo,
	SetServerInfo,
	PrintClientKeyError,
};

enum class GameDllClientKeyValueAction
{
	SkipProtectedInfo,
	SkipInvalidClient,
	SkipUnchanged,
	UpdateAndResend,
};

enum class GameDllClientStringAction
{
	ReturnValue,
	PrintNonClientReturnEmpty,
	PrintNonClientSkip,
};

enum class GameDllQueryCvarAction
{
	SkipEmptyName,
	SendQuery,
	NotifyBadPlayer,
};

enum class GameDllGameDirAction
{
	WriteGameFolder,
	WriteFullPath,
};

struct GameDllSetValuePlan
{
	GameDllSetValueAction action;
	int maxLength;
};

struct GameDllClientKeyValuePlan
{
	GameDllClientKeyValueAction action;
	int clientIndex;
};

struct GameDllPlayerStats
{
	int ping;
	int packetLoss;
};

GameDllInfoBufferRoute BuildGameDllInfoBufferRoute(
	bool validEdict,
	bool worldEdict,
	bool hasClient);
GameDllSetValuePlan BuildGameDllSetValuePlan(
	bool localInfoBuffer,
	bool serverInfoBuffer,
	int maxLocalInfoLength,
	int maxServerInfoLength);
GameDllClientKeyValuePlan BuildGameDllClientKeyValuePlan(
	bool protectedInfoBuffer,
	bool clientsAvailable,
	int clientIndexOneBased,
	int maxClients,
	bool valueChanged);
GameDllClientStringAction BuildGameDllClientStringAction(bool hasClient);
GameDllClientStringAction BuildGameDllClientMutationAction(bool hasClient);
int BuildGameDllPlayerUserId(bool hasClient, int userId);
GameDllPlayerStats BuildGameDllPlayerStats(
	bool hasClient,
	float latency,
	int packetLoss);
GameDllQueryCvarAction BuildGameDllQueryCvarAction(
	const char *cvarName,
	bool hasClient);
GameDllGameDirAction BuildGameDllGameDirAction(
	bool fullPathCompatibility,
	bool rootAvailable,
	bool fullPathFits);

const char *GameDllInfoBufferRouteName(GameDllInfoBufferRoute route);
const char *GameDllSetValueActionName(GameDllSetValueAction action);
const char *GameDllClientKeyValueActionName(GameDllClientKeyValueAction action);
const char *GameDllClientStringActionName(GameDllClientStringAction action);
const char *GameDllQueryCvarActionName(GameDllQueryCvarAction action);
const char *GameDllGameDirActionName(GameDllGameDirAction action);

}
}
}

#endif
