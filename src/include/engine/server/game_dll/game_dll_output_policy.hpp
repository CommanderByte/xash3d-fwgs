#ifndef XASH_ENGINE_SERVER_GAME_DLL_OUTPUT_POLICY_HPP
#define XASH_ENGINE_SERVER_GAME_DLL_OUTPUT_POLICY_HPP

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kGameDllAlertNotice = 0;
constexpr int kGameDllAlertConsole = 1;
constexpr int kGameDllAlertAiConsole = 2;
constexpr int kGameDllAlertWarning = 3;
constexpr int kGameDllAlertError = 4;
constexpr int kGameDllAlertLogged = 5;

constexpr int kGameDllPrintConsole = 0;
constexpr int kGameDllPrintCenter = 1;
constexpr int kGameDllPrintChat = 2;

constexpr int kGameDllDeveloperNone = 0;
constexpr int kGameDllDeveloperExtended = 2;

enum class GameDllServerCommandAction
{
	QueueCommand,
	PrintBadCommand
};

enum class GameDllClientCommandAction
{
	SkipInactiveServer,
	PrintClientNotSpawned,
	SkipFakeClient,
	StuffText,
	PrintBadCommand
};

enum class GameDllClientPrintfAction
{
	PrintNonClientError,
	SkipFakeClient,
	ClientPrintf,
	CenterPrint,
	Ignore
};

enum class GameDllServerPrintAction
{
	ConsolePrint,
	BroadcastPrint
};

enum class GameDllAlertOutputAction
{
	SuppressDeveloper,
	SuppressAiConsole,
	Log,
	PrintNotice,
	PrintConsole,
	PrintAiConsole,
	PrintWarning,
	PrintError,
	Ignore
};

enum class GameDllEndSectionAction
{
	ShowCredits,
	Disconnect
};

GameDllServerCommandAction BuildGameDllServerCommandAction(
	bool commandValid);
GameDllClientCommandAction BuildGameDllClientCommandAction(
	bool serverActive,
	bool hasClient,
	bool fakeClient,
	bool commandValid);
GameDllClientPrintfAction BuildGameDllClientPrintfAction(
	bool hasClient,
	bool fakeClient,
	int printType);
GameDllServerPrintAction BuildGameDllServerPrintAction(
	bool quakeCompatible);
GameDllAlertOutputAction BuildGameDllAlertOutputAction(
	int alertType,
	int maxClients,
	float developerLevel);
GameDllEndSectionAction BuildGameDllEndSectionAction(
	const char *sectionName);

const char *GameDllServerCommandActionName(GameDllServerCommandAction action);
const char *GameDllClientCommandActionName(GameDllClientCommandAction action);
const char *GameDllClientPrintfActionName(GameDllClientPrintfAction action);
const char *GameDllServerPrintActionName(GameDllServerPrintAction action);
const char *GameDllAlertOutputActionName(GameDllAlertOutputAction action);
const char *GameDllEndSectionActionName(GameDllEndSectionAction action);

}
}
}

#endif
