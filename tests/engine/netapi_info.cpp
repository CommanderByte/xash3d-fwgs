#include <cstdlib>
#include <cstring>

#include "engine/server/client/netapi_info.hpp"

using namespace xash::engine::server;

static bool ExpectString(const char *actual, const char *expected)
{
	return std::strcmp(actual, expected) == 0;
}

static bool TestShortServerInfo()
{
	char out[512];
	LegacyServerInfo info = {};

	info.requestProtocol = 49;
	info.protocolVersion = 49;
	info.hostname = "Test Host";
	info.mapName = "crossfire";
	info.deathmatch = true;
	info.teamplay = false;
	info.coop = false;
	info.playerCount = 2;
	info.maxPlayers = 8;
	info.gameFolder = "valve";
	info.passwordProtected = true;

	if (!BuildLegacyServerInfoString(out, sizeof(out), info))
		return false;

	if (!ExpectString(
			out,
			"\\p\\49\\map\\crossfire\\dm\\1\\team\\0\\coop\\0"
			"\\numcl\\2\\maxcl\\8\\gamedir\\valve\\password\\1\\host\\Test Host"))
	{
		return false;
	}

	info.requestProtocol = 48;
	if (!BuildLegacyServerInfoString(out, sizeof(out), info))
		return false;

	return ExpectString(out, "Test Host: wrong version\n");
}

static bool TestErrorsAndPing()
{
	char out[128];

	if (!BuildNetApiPing(out, sizeof(out)) || !ExpectString(out, ""))
		return false;

	if (!BuildNetApiProtocolError(out, sizeof(out)) ||
		!ExpectString(out, "\\neterror\\protocol"))
		return false;

	if (!BuildNetApiUndefinedError(out, sizeof(out)) ||
		!ExpectString(out, "\\neterror\\undefined"))
		return false;

	return BuildNetApiForbiddenError(out, sizeof(out)) &&
		ExpectString(out, "\\neterror\\forbidden");
}

static bool TestRules()
{
	char out[512];
	const NetApiRuleInfo publicRule = { "hostname", "Test Host", false };
	const NetApiRuleInfo protectedRule = { "rcon_password", "secret", true };
	const NetApiRuleInfo emptyProtectedRule = { "sv_password", "", true };
	const NetApiRuleInfo noneProtectedRule = { "fallback_password", "NoNe", true };

	if (!BeginNetApiRules(out, sizeof(out)) ||
		!AppendNetApiRule(out, sizeof(out), publicRule) ||
		!AppendNetApiRule(out, sizeof(out), protectedRule) ||
		!AppendNetApiRule(out, sizeof(out), emptyProtectedRule) ||
		!AppendNetApiRule(out, sizeof(out), noneProtectedRule) ||
		!FinishNetApiRules(out, sizeof(out), 4))
	{
		return false;
	}

	return ExpectString(
		out,
		"\\hostname\\Test Host\\rcon_password\\1\\sv_password\\0"
		"\\fallback_password\\0\\rules\\4");
}

static bool TestPlayers()
{
	char out[512];
	const NetApiPlayerInfo alice = { "Alice", 12, 3.5f };
	const NetApiPlayerInfo bot = { "Bot", -1, -1.0f };

	if (!BeginNetApiPlayers(out, sizeof(out)) ||
		!AppendNetApiPlayer(out, sizeof(out), 0, alice) ||
		!AppendNetApiPlayer(out, sizeof(out), 1, bot) ||
		!FinishNetApiPlayers(out, sizeof(out), 2))
	{
		return false;
	}

	return ExpectString(
		out,
		"\\p0name\\Alice\\p0frags\\12\\p0time\\3.500000"
		"\\p1name\\Bot\\p1frags\\-1\\p1time\\-1.000000\\players\\2");
}

static bool TestDetails()
{
	char out[256];
	NetApiDetailsInfo details = {};

	details.hostname = "Test Host";
	details.gameFolder = "valve";
	details.currentPlayers = 3;
	details.maxPlayers = 8;
	details.mapName = "crossfire";

	return BuildNetApiDetails(out, sizeof(out), details) &&
		ExpectString(
			out,
			"\\hostname\\Test Host\\gamedir\\valve\\current\\3\\max\\8\\map\\crossfire");
}

int main()
{
	if (!TestShortServerInfo() ||
		!TestErrorsAndPing() ||
		!TestRules() ||
		!TestPlayers() ||
		!TestDetails())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
