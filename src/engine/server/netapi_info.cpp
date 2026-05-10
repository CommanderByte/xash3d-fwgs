#include "engine/server/netapi_info.hpp"

#include "engine/info_string.hpp"
#include "engine/server/source_query.hpp"

#include <cstdio>
#include <cstring>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool SetValue(char *out, std::size_t capacity, const char *key, const char *value)
{
	return xash::engine::InfoStringSetValueForKey(
		out,
		key,
		value ? value : "",
		static_cast<int>(capacity)).ok;
}

bool SetInt(char *out, std::size_t capacity, const char *key, int value)
{
	char text[32];
	std::snprintf(text, sizeof(text), "%i", value);
	return SetValue(out, capacity, key, text);
}

bool SetFloat(char *out, std::size_t capacity, const char *key, float value)
{
	char text[64];
	std::snprintf(text, sizeof(text), "%f", value);
	return SetValue(out, capacity, key, text);
}

void ClearOutput(char *out, std::size_t capacity)
{
	if (out && capacity > 0)
		out[0] = '\0';
}

bool BuildNetError(char *out, std::size_t capacity, const char *error)
{
	ClearOutput(out, capacity);
	return SetValue(out, capacity, "neterror", error);
}

}

bool BuildLegacyServerInfoString(
	char *out,
	std::size_t capacity,
	const LegacyServerInfo &info)
{
	if (!out || capacity == 0)
		return false;

	ClearOutput(out, capacity);

	if (info.requestProtocol != info.protocolVersion)
	{
		std::snprintf(out, capacity, "%s: wrong version\n", info.hostname ? info.hostname : "");
		return true;
	}

	if (!SetInt(out, capacity, "p", info.protocolVersion) ||
		!SetValue(out, capacity, "map", info.mapName) ||
		!SetValue(out, capacity, "dm", info.deathmatch ? "1" : "0") ||
		!SetValue(out, capacity, "team", info.teamplay ? "1" : "0") ||
		!SetValue(out, capacity, "coop", info.coop ? "1" : "0") ||
		!SetInt(out, capacity, "numcl", info.playerCount) ||
		!SetInt(out, capacity, "maxcl", info.maxPlayers) ||
		!SetValue(out, capacity, "gamedir", info.gameFolder) ||
		!SetValue(out, capacity, "password", info.passwordProtected ? "1" : "0"))
	{
		return false;
	}

	const int remaining = static_cast<int>(capacity) -
		static_cast<int>(std::strlen(out)) -
		static_cast<int>(sizeof("\\host\\")) -
		1;

	if (remaining < 0)
		return false;

	const int hostCharacters = remaining > 0 ? remaining - 1 : 0;
	char host[512];
	std::snprintf(host, sizeof(host), "%.*s", hostCharacters, info.hostname ? info.hostname : "");
	return SetValue(out, capacity, "host", host);
}

bool BuildNetApiProtocolError(char *out, std::size_t capacity)
{
	return BuildNetError(out, capacity, "protocol");
}

bool BuildNetApiUndefinedError(char *out, std::size_t capacity)
{
	return BuildNetError(out, capacity, "undefined");
}

bool BuildNetApiForbiddenError(char *out, std::size_t capacity)
{
	return BuildNetError(out, capacity, "forbidden");
}

bool BuildNetApiPing(char *out, std::size_t capacity)
{
	ClearOutput(out, capacity);
	return out && capacity > 0;
}

bool BeginNetApiRules(char *out, std::size_t capacity)
{
	ClearOutput(out, capacity);
	return out && capacity > 0;
}

bool AppendNetApiRule(
	char *out,
	std::size_t capacity,
	const NetApiRuleInfo &rule)
{
	return SetValue(
		out,
		capacity,
		rule.name,
		rule.isProtected ? SourceQueryProtectedValue(rule.value) : rule.value);
}

bool FinishNetApiRules(char *out, std::size_t capacity, int count)
{
	return SetInt(out, capacity, "rules", count);
}

bool BeginNetApiPlayers(char *out, std::size_t capacity)
{
	ClearOutput(out, capacity);
	return out && capacity > 0;
}

bool AppendNetApiPlayer(
	char *out,
	std::size_t capacity,
	int index,
	const NetApiPlayerInfo &player)
{
	char key[32];

	std::snprintf(key, sizeof(key), "p%iname", index);
	if (!SetValue(out, capacity, key, player.name))
		return false;

	std::snprintf(key, sizeof(key), "p%ifrags", index);
	if (!SetInt(out, capacity, key, player.frags))
		return false;

	std::snprintf(key, sizeof(key), "p%itime", index);
	return SetFloat(out, capacity, key, player.time);
}

bool FinishNetApiPlayers(char *out, std::size_t capacity, int count)
{
	return SetInt(out, capacity, "players", count);
}

bool BuildNetApiDetails(
	char *out,
	std::size_t capacity,
	const NetApiDetailsInfo &details)
{
	ClearOutput(out, capacity);

	return SetValue(out, capacity, "hostname", details.hostname) &&
		SetValue(out, capacity, "gamedir", details.gameFolder) &&
		SetInt(out, capacity, "current", details.currentPlayers) &&
		SetInt(out, capacity, "max", details.maxPlayers) &&
		SetValue(out, capacity, "map", details.mapName);
}

}
}
}
