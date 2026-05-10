#include "engine/server/game_dll_output_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool EqualIgnoreAsciiCase(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs)
		return false;

	while (*lhs && *rhs)
	{
		char left = *lhs++;
		char right = *rhs++;

		if (left >= 'A' && left <= 'Z')
			left = static_cast<char>(left - 'A' + 'a');

		if (right >= 'A' && right <= 'Z')
			right = static_cast<char>(right - 'A' + 'a');

		if (left != right)
			return false;
	}

	return *lhs == 0 && *rhs == 0;
}

}

GameDllServerCommandAction BuildGameDllServerCommandAction(
	bool commandValid)
{
	return commandValid
		? GameDllServerCommandAction::QueueCommand
		: GameDllServerCommandAction::PrintBadCommand;
}

GameDllClientCommandAction BuildGameDllClientCommandAction(
	bool serverActive,
	bool hasClient,
	bool fakeClient,
	bool commandValid)
{
	if (!serverActive)
		return GameDllClientCommandAction::SkipInactiveServer;

	if (!hasClient)
		return GameDllClientCommandAction::PrintClientNotSpawned;

	if (fakeClient)
		return GameDllClientCommandAction::SkipFakeClient;

	return commandValid
		? GameDllClientCommandAction::StuffText
		: GameDllClientCommandAction::PrintBadCommand;
}

GameDllClientPrintfAction BuildGameDllClientPrintfAction(
	bool hasClient,
	bool fakeClient,
	int printType)
{
	if (!hasClient)
		return GameDllClientPrintfAction::PrintNonClientError;

	if (fakeClient)
		return GameDllClientPrintfAction::SkipFakeClient;

	switch (printType)
	{
	case kGameDllPrintConsole:
	case kGameDllPrintChat:
		return GameDllClientPrintfAction::ClientPrintf;
	case kGameDllPrintCenter:
		return GameDllClientPrintfAction::CenterPrint;
	default:
		return GameDllClientPrintfAction::Ignore;
	}
}

GameDllServerPrintAction BuildGameDllServerPrintAction(
	bool quakeCompatible)
{
	return quakeCompatible
		? GameDllServerPrintAction::BroadcastPrint
		: GameDllServerPrintAction::ConsolePrint;
}

GameDllAlertOutputAction BuildGameDllAlertOutputAction(
	int alertType,
	int maxClients,
	float developerLevel)
{
	if (alertType == kGameDllAlertLogged && maxClients > 1)
		return GameDllAlertOutputAction::Log;

	if (developerLevel <= kGameDllDeveloperNone)
		return GameDllAlertOutputAction::SuppressDeveloper;

	if (alertType == kGameDllAlertAiConsole &&
		developerLevel < kGameDllDeveloperExtended)
	{
		return GameDllAlertOutputAction::SuppressAiConsole;
	}

	switch (alertType)
	{
	case kGameDllAlertNotice:
		return GameDllAlertOutputAction::PrintNotice;
	case kGameDllAlertConsole:
		return GameDllAlertOutputAction::PrintConsole;
	case kGameDllAlertAiConsole:
		return GameDllAlertOutputAction::PrintAiConsole;
	case kGameDllAlertWarning:
		return GameDllAlertOutputAction::PrintWarning;
	case kGameDllAlertError:
		return GameDllAlertOutputAction::PrintError;
	default:
		return GameDllAlertOutputAction::Ignore;
	}
}

GameDllEndSectionAction BuildGameDllEndSectionAction(
	const char *sectionName)
{
	return EqualIgnoreAsciiCase(sectionName, "oem_end_credits")
		? GameDllEndSectionAction::ShowCredits
		: GameDllEndSectionAction::Disconnect;
}

const char *GameDllServerCommandActionName(GameDllServerCommandAction action)
{
	switch (action)
	{
	case GameDllServerCommandAction::QueueCommand:
		return "queue-command";
	case GameDllServerCommandAction::PrintBadCommand:
		return "print-bad-command";
	}

	return "unknown";
}

const char *GameDllClientCommandActionName(GameDllClientCommandAction action)
{
	switch (action)
	{
	case GameDllClientCommandAction::SkipInactiveServer:
		return "skip-inactive-server";
	case GameDllClientCommandAction::PrintClientNotSpawned:
		return "print-client-not-spawned";
	case GameDllClientCommandAction::SkipFakeClient:
		return "skip-fake-client";
	case GameDllClientCommandAction::StuffText:
		return "stuff-text";
	case GameDllClientCommandAction::PrintBadCommand:
		return "print-bad-command";
	}

	return "unknown";
}

const char *GameDllClientPrintfActionName(GameDllClientPrintfAction action)
{
	switch (action)
	{
	case GameDllClientPrintfAction::PrintNonClientError:
		return "print-non-client-error";
	case GameDllClientPrintfAction::SkipFakeClient:
		return "skip-fake-client";
	case GameDllClientPrintfAction::ClientPrintf:
		return "client-printf";
	case GameDllClientPrintfAction::CenterPrint:
		return "center-print";
	case GameDllClientPrintfAction::Ignore:
		return "ignore";
	}

	return "unknown";
}

const char *GameDllServerPrintActionName(GameDllServerPrintAction action)
{
	switch (action)
	{
	case GameDllServerPrintAction::ConsolePrint:
		return "console-print";
	case GameDllServerPrintAction::BroadcastPrint:
		return "broadcast-print";
	}

	return "unknown";
}

const char *GameDllAlertOutputActionName(GameDllAlertOutputAction action)
{
	switch (action)
	{
	case GameDllAlertOutputAction::SuppressDeveloper:
		return "suppress-developer";
	case GameDllAlertOutputAction::SuppressAiConsole:
		return "suppress-ai-console";
	case GameDllAlertOutputAction::Log:
		return "log";
	case GameDllAlertOutputAction::PrintNotice:
		return "print-notice";
	case GameDllAlertOutputAction::PrintConsole:
		return "print-console";
	case GameDllAlertOutputAction::PrintAiConsole:
		return "print-ai-console";
	case GameDllAlertOutputAction::PrintWarning:
		return "print-warning";
	case GameDllAlertOutputAction::PrintError:
		return "print-error";
	case GameDllAlertOutputAction::Ignore:
		return "ignore";
	}

	return "unknown";
}

const char *GameDllEndSectionActionName(GameDllEndSectionAction action)
{
	switch (action)
	{
	case GameDllEndSectionAction::ShowCredits:
		return "show-credits";
	case GameDllEndSectionAction::Disconnect:
		return "disconnect";
	}

	return "unknown";
}

}
}
}
