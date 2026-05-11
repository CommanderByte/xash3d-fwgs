#include "engine/server/game_dll/game_dll_client_info_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool EmptyOrNull(const char *value)
{
	return !value || value[0] == '\0';
}

}

GameDllInfoBufferRoute BuildGameDllInfoBufferRoute(
	bool validEdict,
	bool worldEdict,
	bool hasClient)
{
	if (!validEdict)
		return GameDllInfoBufferRoute::LocalInfo;

	if (worldEdict)
		return GameDllInfoBufferRoute::ServerInfo;

	if (hasClient)
		return GameDllInfoBufferRoute::ClientUserinfo;

	return GameDllInfoBufferRoute::EmptyString;
}

GameDllSetValuePlan BuildGameDllSetValuePlan(
	bool localInfoBuffer,
	bool serverInfoBuffer,
	int maxLocalInfoLength,
	int maxServerInfoLength)
{
	GameDllSetValuePlan plan = {};

	if (localInfoBuffer)
	{
		plan.action = GameDllSetValueAction::SetLocalInfo;
		plan.maxLength = maxLocalInfoLength;
		return plan;
	}

	if (serverInfoBuffer)
	{
		plan.action = GameDllSetValueAction::SetServerInfo;
		plan.maxLength = maxServerInfoLength;
		return plan;
	}

	plan.action = GameDllSetValueAction::PrintClientKeyError;
	return plan;
}

GameDllClientKeyValuePlan BuildGameDllClientKeyValuePlan(
	bool protectedInfoBuffer,
	bool clientsAvailable,
	int clientIndexOneBased,
	int maxClients,
	bool valueChanged)
{
	GameDllClientKeyValuePlan plan = {};
	plan.clientIndex = clientIndexOneBased - 1;

	if (protectedInfoBuffer)
	{
		plan.action = GameDllClientKeyValueAction::SkipProtectedInfo;
		return plan;
	}

	if (!clientsAvailable || plan.clientIndex < 0 || plan.clientIndex >= maxClients)
	{
		plan.action = GameDllClientKeyValueAction::SkipInvalidClient;
		return plan;
	}

	if (!valueChanged)
	{
		plan.action = GameDllClientKeyValueAction::SkipUnchanged;
		return plan;
	}

	plan.action = GameDllClientKeyValueAction::UpdateAndResend;
	return plan;
}

GameDllClientStringAction BuildGameDllClientStringAction(bool hasClient)
{
	return hasClient ?
		GameDllClientStringAction::ReturnValue :
		GameDllClientStringAction::PrintNonClientReturnEmpty;
}

GameDllClientStringAction BuildGameDllClientMutationAction(bool hasClient)
{
	return hasClient ?
		GameDllClientStringAction::ReturnValue :
		GameDllClientStringAction::PrintNonClientSkip;
}

int BuildGameDllPlayerUserId(bool hasClient, int userId)
{
	return hasClient ? userId : -1;
}

GameDllPlayerStats BuildGameDllPlayerStats(
	bool hasClient,
	float latency,
	int packetLoss)
{
	GameDllPlayerStats stats = {};

	if (!hasClient)
		return stats;

	stats.ping = static_cast<int>(latency * 1000.0f);
	stats.packetLoss = packetLoss;
	return stats;
}

GameDllQueryCvarAction BuildGameDllQueryCvarAction(
	const char *cvarName,
	bool hasClient)
{
	if (EmptyOrNull(cvarName))
		return GameDllQueryCvarAction::SkipEmptyName;

	if (!hasClient)
		return GameDllQueryCvarAction::NotifyBadPlayer;

	return GameDllQueryCvarAction::SendQuery;
}

GameDllGameDirAction BuildGameDllGameDirAction(
	bool fullPathCompatibility,
	bool rootAvailable,
	bool fullPathFits)
{
	if (fullPathCompatibility && rootAvailable && fullPathFits)
		return GameDllGameDirAction::WriteFullPath;

	return GameDllGameDirAction::WriteGameFolder;
}

const char *GameDllInfoBufferRouteName(GameDllInfoBufferRoute route)
{
	switch (route)
	{
	case GameDllInfoBufferRoute::LocalInfo:
		return "localinfo";
	case GameDllInfoBufferRoute::ServerInfo:
		return "serverinfo";
	case GameDllInfoBufferRoute::ClientUserinfo:
		return "client-userinfo";
	case GameDllInfoBufferRoute::EmptyString:
		return "empty-string";
	}

	return "unknown";
}

const char *GameDllSetValueActionName(GameDllSetValueAction action)
{
	switch (action)
	{
	case GameDllSetValueAction::SetLocalInfo:
		return "set-localinfo";
	case GameDllSetValueAction::SetServerInfo:
		return "set-serverinfo";
	case GameDllSetValueAction::PrintClientKeyError:
		return "print-client-key-error";
	}

	return "unknown";
}

const char *GameDllClientKeyValueActionName(GameDllClientKeyValueAction action)
{
	switch (action)
	{
	case GameDllClientKeyValueAction::SkipProtectedInfo:
		return "skip-protected-info";
	case GameDllClientKeyValueAction::SkipInvalidClient:
		return "skip-invalid-client";
	case GameDllClientKeyValueAction::SkipUnchanged:
		return "skip-unchanged";
	case GameDllClientKeyValueAction::UpdateAndResend:
		return "update-and-resend";
	}

	return "unknown";
}

const char *GameDllClientStringActionName(GameDllClientStringAction action)
{
	switch (action)
	{
	case GameDllClientStringAction::ReturnValue:
		return "return-value";
	case GameDllClientStringAction::PrintNonClientReturnEmpty:
		return "print-non-client-return-empty";
	case GameDllClientStringAction::PrintNonClientSkip:
		return "print-non-client-skip";
	}

	return "unknown";
}

const char *GameDllQueryCvarActionName(GameDllQueryCvarAction action)
{
	switch (action)
	{
	case GameDllQueryCvarAction::SkipEmptyName:
		return "skip-empty-name";
	case GameDllQueryCvarAction::SendQuery:
		return "send-query";
	case GameDllQueryCvarAction::NotifyBadPlayer:
		return "notify-bad-player";
	}

	return "unknown";
}

const char *GameDllGameDirActionName(GameDllGameDirAction action)
{
	switch (action)
	{
	case GameDllGameDirAction::WriteGameFolder:
		return "write-game-folder";
	case GameDllGameDirAction::WriteFullPath:
		return "write-full-path";
	}

	return "unknown";
}

}
}
}
