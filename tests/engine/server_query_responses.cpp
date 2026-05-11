#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/server/client/netapi_info.hpp"
#include "engine/server/client/source_query.hpp"

using namespace xash::engine::server;

namespace
{

static void AppendString(std::vector<uint8_t> &bytes, const char *value)
{
	while (*value)
	{
		bytes.push_back(static_cast<uint8_t>(*value));
		++value;
	}

	bytes.push_back(0);
}

static void AppendByte(std::vector<uint8_t> &bytes, uint8_t value)
{
	bytes.push_back(value);
}

static void AppendShort(std::vector<uint8_t> &bytes, uint16_t value)
{
	bytes.push_back(static_cast<uint8_t>(value & 0xffU));
	bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xffU));
}

static void AppendDword(std::vector<uint8_t> &bytes, uint32_t value)
{
	bytes.push_back(static_cast<uint8_t>(value & 0xffU));
	bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xffU));
	bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xffU));
	bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xffU));
}

static void AppendLong(std::vector<uint8_t> &bytes, int32_t value)
{
	AppendDword(bytes, static_cast<uint32_t>(value));
}

static void AppendFloat(std::vector<uint8_t> &bytes, float value)
{
	uint32_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	AppendDword(bytes, bits);
}

static bool MatchesBuffer(
	const uint8_t *buffer,
	std::size_t bufferSize,
	const std::vector<uint8_t> &expected)
{
	return bufferSize == expected.size() &&
		std::memcmp(buffer, expected.data(), expected.size()) == 0;
}

static bool ExpectString(const char *actual, const char *expected)
{
	return std::strcmp(actual, expected) == 0;
}

static bool TestSourceQueryDetailsPayload()
{
	uint8_t buffer[256] = {};
	SourceQueryDetails details = {};

	details.protocolVersion = 49;
	details.hostname = "Test Host";
	details.mapName = "crossfire";
	details.gameFolder = "valve";
	details.gameDescription = "Half-Life";
	details.appId = 0;
	details.playerCount = 3;
	details.maxPlayers = 8;
	details.botCount = 1;
	details.serverType = 'd';
	details.platform = 'w';
	details.passwordProtected = true;
	details.secure = 0;
	details.version = "0.21";

	const std::size_t bytes =
		BuildSourceQueryDetails(details, buffer, sizeof(buffer));

	std::vector<uint8_t> expected;
	AppendDword(expected, 0xffffffffU);
	AppendByte(expected, 'I');
	AppendByte(expected, 49);
	AppendString(expected, "Test Host");
	AppendString(expected, "crossfire");
	AppendString(expected, "valve");
	AppendString(expected, "Half-Life");
	AppendShort(expected, 0);
	AppendByte(expected, 3);
	AppendByte(expected, 8);
	AppendByte(expected, 1);
	AppendByte(expected, 'd');
	AppendByte(expected, 'w');
	AppendByte(expected, 1);
	AppendByte(expected, 0);
	AppendString(expected, "0.21");

	return MatchesBuffer(buffer, bytes, expected);
}

static bool TestSourceQueryRulesPayload()
{
	uint8_t buffer[256] = {};
	const SourceQueryRule rules[] =
	{
		{ "hostname", "Test Host", false },
		{ "rcon_password", "secret", true },
		{ "sv_password", "", true },
		{ "fallback_password", "NoNe", true },
	};

	const std::size_t bytes =
		BuildSourceQueryRules(rules, 4, buffer, sizeof(buffer));

	std::vector<uint8_t> expected;
	AppendDword(expected, 0xffffffffU);
	AppendByte(expected, 'E');
	AppendShort(expected, 4);
	AppendString(expected, "hostname");
	AppendString(expected, "Test Host");
	AppendString(expected, "rcon_password");
	AppendString(expected, "1");
	AppendString(expected, "sv_password");
	AppendString(expected, "0");
	AppendString(expected, "fallback_password");
	AppendString(expected, "0");

	return MatchesBuffer(buffer, bytes, expected) &&
		BuildSourceQueryRules(nullptr, 0, buffer, sizeof(buffer)) == 0 &&
		std::strcmp(SourceQueryProtectedValue("secret"), "1") == 0 &&
		std::strcmp(SourceQueryProtectedValue("none"), "0") == 0 &&
		std::strcmp(SourceQueryProtectedValue("NoNe"), "0") == 0 &&
		std::strcmp(SourceQueryProtectedValue(""), "0") == 0 &&
		std::strcmp(SourceQueryProtectedValue(nullptr), "0") == 0;
}

static bool TestSourceQueryPlayersPayload()
{
	uint8_t buffer[256] = {};
	const SourceQueryPlayer players[] =
	{
		{ 0, "Alice", 12, 3.5f },
		{ 1, "Bot", -1, -1.0f },
	};

	const std::size_t bytes =
		BuildSourceQueryPlayers(players, 2, buffer, sizeof(buffer));

	std::vector<uint8_t> expected;
	AppendDword(expected, 0xffffffffU);
	AppendByte(expected, 'D');
	AppendByte(expected, 2);
	AppendByte(expected, 0);
	AppendString(expected, "Alice");
	AppendLong(expected, 12);
	AppendFloat(expected, 3.5f);
	AppendByte(expected, 1);
	AppendString(expected, "Bot");
	AppendLong(expected, -1);
	AppendFloat(expected, -1.0f);

	return MatchesBuffer(buffer, bytes, expected) &&
		BuildSourceQueryPlayers(nullptr, 0, buffer, sizeof(buffer)) == 0 &&
		SourceQueryAllowsPlayerList(true, false) &&
		!SourceQueryAllowsPlayerList(false, false) &&
		!SourceQueryAllowsPlayerList(true, true);
}

static bool TestSourceQueryStreamingRules()
{
	uint8_t buffer[128] = {};
	SourceQueryRule rule = {};
	rule.name = "sv_gravity";
	rule.value = "800";

	std::size_t offset = BeginSourceQueryRules(buffer, sizeof(buffer));

	if (!offset)
		return false;

	offset = AppendSourceQueryRule(buffer, sizeof(buffer), offset, rule);
	if (!offset)
		return false;

	const std::size_t bytes =
		FinishSourceQueryRules(buffer, sizeof(buffer), offset, 1);

	std::vector<uint8_t> expected;
	AppendDword(expected, 0xffffffffU);
	AppendByte(expected, 'E');
	AppendShort(expected, 1);
	AppendString(expected, "sv_gravity");
	AppendString(expected, "800");

	return MatchesBuffer(buffer, bytes, expected);
}

static bool TestNetApiShortServerInfo()
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
			"\\numcl\\2\\maxcl\\8\\gamedir\\valve\\password\\1"
			"\\host\\Test Host"))
	{
		return false;
	}

	info.requestProtocol = 48;
	if (!BuildLegacyServerInfoString(out, sizeof(out), info))
		return false;

	return ExpectString(out, "Test Host: wrong version\n");
}

static bool TestNetApiErrorsAndPing()
{
	char out[128];

	if (!BuildNetApiPing(out, sizeof(out)) || !ExpectString(out, ""))
		return false;

	if (!BuildNetApiProtocolError(out, sizeof(out)) ||
		!ExpectString(out, "\\neterror\\protocol"))
	{
		return false;
	}

	if (!BuildNetApiUndefinedError(out, sizeof(out)) ||
		!ExpectString(out, "\\neterror\\undefined"))
	{
		return false;
	}

	return BuildNetApiForbiddenError(out, sizeof(out)) &&
		ExpectString(out, "\\neterror\\forbidden");
}

static bool TestNetApiRules()
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

static bool TestNetApiPlayers()
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

static bool TestNetApiDetails()
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
			"\\hostname\\Test Host\\gamedir\\valve"
			"\\current\\3\\max\\8\\map\\crossfire");
}

}

int main()
{
	if (!TestSourceQueryDetailsPayload() ||
		!TestSourceQueryRulesPayload() ||
		!TestSourceQueryPlayersPayload() ||
		!TestSourceQueryStreamingRules() ||
		!TestNetApiShortServerInfo() ||
		!TestNetApiErrorsAndPing() ||
		!TestNetApiRules() ||
		!TestNetApiPlayers() ||
		!TestNetApiDetails())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
