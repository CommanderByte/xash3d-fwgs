#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_output_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestServerCommandValidation()
{
	return BuildGameDllServerCommandAction(true) ==
			GameDllServerCommandAction::QueueCommand &&
		BuildGameDllServerCommandAction(false) ==
			GameDllServerCommandAction::PrintBadCommand;
}

static bool TestClientCommandRouting()
{
	return BuildGameDllClientCommandAction(false, true, false, true) ==
			GameDllClientCommandAction::SkipInactiveServer &&
		BuildGameDllClientCommandAction(true, false, false, true) ==
			GameDllClientCommandAction::PrintClientNotSpawned &&
		BuildGameDllClientCommandAction(true, true, true, true) ==
			GameDllClientCommandAction::SkipFakeClient &&
		BuildGameDllClientCommandAction(true, true, false, false) ==
			GameDllClientCommandAction::PrintBadCommand &&
		BuildGameDllClientCommandAction(true, true, false, true) ==
			GameDllClientCommandAction::StuffText;
}

static bool TestClientPrintfRouting()
{
	return BuildGameDllClientPrintfAction(false, false, kGameDllPrintConsole) ==
			GameDllClientPrintfAction::PrintNonClientError &&
		BuildGameDllClientPrintfAction(true, true, kGameDllPrintConsole) ==
			GameDllClientPrintfAction::SkipFakeClient &&
		BuildGameDllClientPrintfAction(true, false, kGameDllPrintConsole) ==
			GameDllClientPrintfAction::ClientPrintf &&
		BuildGameDllClientPrintfAction(true, false, kGameDllPrintChat) ==
			GameDllClientPrintfAction::ClientPrintf &&
		BuildGameDllClientPrintfAction(true, false, kGameDllPrintCenter) ==
			GameDllClientPrintfAction::CenterPrint &&
		BuildGameDllClientPrintfAction(true, false, 99) ==
			GameDllClientPrintfAction::Ignore;
}

static bool TestAlertDeveloperAndLogging()
{
	return BuildGameDllAlertOutputAction(
			kGameDllAlertLogged,
			2,
			0.0f) == GameDllAlertOutputAction::Log &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertNotice,
			1,
			0.0f) ==
			GameDllAlertOutputAction::SuppressDeveloper &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertNotice,
			1,
			0.5f) == GameDllAlertOutputAction::PrintNotice &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertLogged,
			1,
			0.0f) ==
			GameDllAlertOutputAction::SuppressDeveloper &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertAiConsole,
			1,
			1.5f) == GameDllAlertOutputAction::SuppressAiConsole &&
		BuildGameDllAlertOutputAction(
			kGameDllAlertAiConsole,
			1,
			2.0f) ==
			GameDllAlertOutputAction::PrintAiConsole;
}

static bool TestAlertPrintClasses()
{
	return BuildGameDllAlertOutputAction(kGameDllAlertNotice, 1, 1) ==
			GameDllAlertOutputAction::PrintNotice &&
		BuildGameDllAlertOutputAction(kGameDllAlertConsole, 1, 1) ==
			GameDllAlertOutputAction::PrintConsole &&
		BuildGameDllAlertOutputAction(kGameDllAlertWarning, 1, 1) ==
			GameDllAlertOutputAction::PrintWarning &&
		BuildGameDllAlertOutputAction(kGameDllAlertError, 1, 1) ==
			GameDllAlertOutputAction::PrintError &&
		BuildGameDllAlertOutputAction(99, 1, 1) ==
			GameDllAlertOutputAction::Ignore;
}

static bool TestServerPrintAndEndSection()
{
	return BuildGameDllServerPrintAction(false) ==
			GameDllServerPrintAction::ConsolePrint &&
		BuildGameDllServerPrintAction(true) ==
			GameDllServerPrintAction::BroadcastPrint &&
		BuildGameDllEndSectionAction("oem_end_credits") ==
			GameDllEndSectionAction::ShowCredits &&
		BuildGameDllEndSectionAction("OEM_END_CREDITS") ==
			GameDllEndSectionAction::ShowCredits &&
		BuildGameDllEndSectionAction(nullptr) ==
			GameDllEndSectionAction::Disconnect &&
		BuildGameDllEndSectionAction("chapter_1") ==
			GameDllEndSectionAction::Disconnect;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllClientCommandActionName(
				GameDllClientCommandAction::SkipFakeClient),
			"skip-fake-client") == 0 &&
		std::strcmp(
			GameDllAlertOutputActionName(
				GameDllAlertOutputAction::SuppressAiConsole),
			"suppress-ai-console") == 0 &&
		std::strcmp(
			GameDllEndSectionActionName(
				GameDllEndSectionAction::ShowCredits),
			"show-credits") == 0;
}

}

int main()
{
	if (!TestServerCommandValidation() ||
		!TestClientCommandRouting() ||
		!TestClientPrintfRouting() ||
		!TestAlertDeveloperAndLogging() ||
		!TestAlertPrintClasses() ||
		!TestServerPrintAndEndSection() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
