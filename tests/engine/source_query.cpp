#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/server/client/source_query.hpp"

using namespace xash::engine::server;

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

static bool TestDetailsPayload()
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

	const std::size_t bytes = BuildSourceQueryDetails(details, buffer, sizeof(buffer));

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

static bool TestRulesPayload()
{
	uint8_t buffer[256] = {};
	const SourceQueryRule rules[] =
	{
		{ "hostname", "Test Host", false },
		{ "rcon_password", "secret", true },
		{ "sv_password", "", true },
		{ "fallback_password", "NoNe", true },
	};

	const std::size_t bytes = BuildSourceQueryRules(rules, 4, buffer, sizeof(buffer));

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

static bool TestPlayersPayload()
{
	uint8_t buffer[256] = {};
	const SourceQueryPlayer players[] =
	{
		{ 0, "Alice", 12, 3.5f },
		{ 1, "Bot", -1, -1.0f },
	};

	const std::size_t bytes = BuildSourceQueryPlayers(players, 2, buffer, sizeof(buffer));

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

static bool TestStreamingRules()
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

	const std::size_t bytes = FinishSourceQueryRules(buffer, sizeof(buffer), offset, 1);

	std::vector<uint8_t> expected;
	AppendDword(expected, 0xffffffffU);
	AppendByte(expected, 'E');
	AppendShort(expected, 1);
	AppendString(expected, "sv_gravity");
	AppendString(expected, "800");

	return MatchesBuffer(buffer, bytes, expected);
}

int main()
{
	if (!TestDetailsPayload() ||
		!TestRulesPayload() ||
		!TestPlayersPayload() ||
		!TestStreamingRules())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
