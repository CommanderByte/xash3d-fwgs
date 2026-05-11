#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_client_info_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestInfoBufferRouting()
{
	return BuildGameDllInfoBufferRoute(false, false, false) ==
			GameDllInfoBufferRoute::LocalInfo &&
		BuildGameDllInfoBufferRoute(true, true, false) ==
			GameDllInfoBufferRoute::ServerInfo &&
		BuildGameDllInfoBufferRoute(true, false, true) ==
			GameDllInfoBufferRoute::ClientUserinfo &&
		BuildGameDllInfoBufferRoute(true, false, false) ==
			GameDllInfoBufferRoute::EmptyString;
}

static bool TestSetValuePlans()
{
	const GameDllSetValuePlan local =
		BuildGameDllSetValuePlan(true, false, 32768, 512);
	const GameDllSetValuePlan server =
		BuildGameDllSetValuePlan(false, true, 32768, 512);
	const GameDllSetValuePlan client =
		BuildGameDllSetValuePlan(false, false, 32768, 512);

	return local.action == GameDllSetValueAction::SetLocalInfo &&
		local.maxLength == 32768 &&
		server.action == GameDllSetValueAction::SetServerInfo &&
		server.maxLength == 512 &&
		client.action == GameDllSetValueAction::PrintClientKeyError &&
		client.maxLength == 0;
}

static bool TestClientKeyValuePlans()
{
	return BuildGameDllClientKeyValuePlan(true, true, 1, 4, true).action ==
			GameDllClientKeyValueAction::SkipProtectedInfo &&
		BuildGameDllClientKeyValuePlan(false, false, 1, 4, true).action ==
			GameDllClientKeyValueAction::SkipInvalidClient &&
		BuildGameDllClientKeyValuePlan(false, true, 0, 4, true).action ==
			GameDllClientKeyValueAction::SkipInvalidClient &&
		BuildGameDllClientKeyValuePlan(false, true, 5, 4, true).action ==
			GameDllClientKeyValueAction::SkipInvalidClient &&
		BuildGameDllClientKeyValuePlan(false, true, 2, 4, false).action ==
			GameDllClientKeyValueAction::SkipUnchanged &&
		BuildGameDllClientKeyValuePlan(false, true, 2, 4, true).action ==
			GameDllClientKeyValueAction::UpdateAndResend &&
		BuildGameDllClientKeyValuePlan(false, true, 2, 4, true).clientIndex == 1;
}

static bool TestClientStringAndMutationPlans()
{
	return BuildGameDllClientStringAction(true) ==
			GameDllClientStringAction::ReturnValue &&
		BuildGameDllClientStringAction(false) ==
			GameDllClientStringAction::PrintNonClientReturnEmpty &&
		BuildGameDllClientMutationAction(true) ==
			GameDllClientStringAction::ReturnValue &&
		BuildGameDllClientMutationAction(false) ==
			GameDllClientStringAction::PrintNonClientSkip;
}

static bool TestPlayerIdentityAndStats()
{
	const GameDllPlayerStats missing =
		BuildGameDllPlayerStats(false, 0.123f, 7);
	const GameDllPlayerStats present =
		BuildGameDllPlayerStats(true, 0.123f, 7);

	return BuildGameDllPlayerUserId(false, 42) == -1 &&
		BuildGameDllPlayerUserId(true, 42) == 42 &&
		missing.ping == 0 &&
		missing.packetLoss == 0 &&
		present.ping == 123 &&
		present.packetLoss == 7;
}

static bool TestQueryClientCvarPlans()
{
	return BuildGameDllQueryCvarAction(nullptr, true) ==
			GameDllQueryCvarAction::SkipEmptyName &&
		BuildGameDllQueryCvarAction("", true) ==
			GameDllQueryCvarAction::SkipEmptyName &&
		BuildGameDllQueryCvarAction("cl_lw", true) ==
			GameDllQueryCvarAction::SendQuery &&
		BuildGameDllQueryCvarAction("cl_lw", false) ==
			GameDllQueryCvarAction::NotifyBadPlayer;
}

static bool TestGameDirFallbackPlans()
{
	return BuildGameDllGameDirAction(false, true, true) ==
			GameDllGameDirAction::WriteGameFolder &&
		BuildGameDllGameDirAction(true, false, true) ==
			GameDllGameDirAction::WriteGameFolder &&
		BuildGameDllGameDirAction(true, true, false) ==
			GameDllGameDirAction::WriteGameFolder &&
		BuildGameDllGameDirAction(true, true, true) ==
			GameDllGameDirAction::WriteFullPath;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllInfoBufferRouteName(GameDllInfoBufferRoute::ClientUserinfo),
			"client-userinfo") == 0 &&
		std::strcmp(
			GameDllSetValueActionName(
				GameDllSetValueAction::PrintClientKeyError),
			"print-client-key-error") == 0 &&
		std::strcmp(
			GameDllClientKeyValueActionName(
				GameDllClientKeyValueAction::UpdateAndResend),
			"update-and-resend") == 0 &&
		std::strcmp(
			GameDllQueryCvarActionName(GameDllQueryCvarAction::NotifyBadPlayer),
			"notify-bad-player") == 0 &&
		std::strcmp(
			GameDllGameDirActionName(GameDllGameDirAction::WriteFullPath),
			"write-full-path") == 0;
}

}

int main()
{
	if (!TestInfoBufferRouting() ||
		!TestSetValuePlans() ||
		!TestClientKeyValuePlans() ||
		!TestClientStringAndMutationPlans() ||
		!TestPlayerIdentityAndStats() ||
		!TestQueryClientCvarPlans() ||
		!TestGameDirFallbackPlans() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
