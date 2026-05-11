#include "game_dll_output_policy_adapter.h"

#include "common.h"
#include "engine/server/game_dll/game_dll_output_policy.hpp"
#include "eiface.h"
#include "game_dll_adapter_shared.hpp"

using xash::engine::server::adapter::FromLegacyBool;
using xash::engine::server::adapter::ToLegacyEnum;

static_assert(at_notice == xash::engine::server::kGameDllAlertNotice,
	"at_notice value changed");
static_assert(at_console == xash::engine::server::kGameDllAlertConsole,
	"at_console value changed");
static_assert(at_aiconsole == xash::engine::server::kGameDllAlertAiConsole,
	"at_aiconsole value changed");
static_assert(at_warning == xash::engine::server::kGameDllAlertWarning,
	"at_warning value changed");
static_assert(at_error == xash::engine::server::kGameDllAlertError,
	"at_error value changed");
static_assert(at_logged == xash::engine::server::kGameDllAlertLogged,
	"at_logged value changed");
static_assert(print_console == xash::engine::server::kGameDllPrintConsole,
	"print_console value changed");
static_assert(print_center == xash::engine::server::kGameDllPrintCenter,
	"print_center value changed");
static_assert(print_chat == xash::engine::server::kGameDllPrintChat,
	"print_chat value changed");
static_assert(DEV_NONE == xash::engine::server::kGameDllDeveloperNone,
	"DEV_NONE value changed");
static_assert(DEV_EXTENDED == xash::engine::server::kGameDllDeveloperExtended,
	"DEV_EXTENDED value changed");

extern "C" int SV_GameDllOutput_BuildServerCommandAction(int command_valid)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllServerCommandAction(
			FromLegacyBool(command_valid)));
}

extern "C" int SV_GameDllOutput_BuildClientCommandAction(
	int server_active,
	int has_client,
	int fake_client,
	int command_valid)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllClientCommandAction(
			FromLegacyBool(server_active),
			FromLegacyBool(has_client),
			FromLegacyBool(fake_client),
			FromLegacyBool(command_valid)));
}

extern "C" int SV_GameDllOutput_BuildClientPrintfAction(
	int has_client,
	int fake_client,
	int print_type)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllClientPrintfAction(
			FromLegacyBool(has_client),
			FromLegacyBool(fake_client),
			print_type));
}

extern "C" int SV_GameDllOutput_BuildServerPrintAction(int quake_compatible)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllServerPrintAction(
			FromLegacyBool(quake_compatible)));
}

extern "C" int SV_GameDllOutput_BuildAlertAction(
	int alert_type,
	int max_clients,
	float developer_level)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllAlertOutputAction(
			alert_type,
			max_clients,
			developer_level));
}

extern "C" int SV_GameDllOutput_BuildEndSectionAction(const char *section_name)
{
	return ToLegacyEnum(
		xash::engine::server::BuildGameDllEndSectionAction(section_name));
}
