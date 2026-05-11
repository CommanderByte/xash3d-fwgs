#ifndef XASH_ENGINE_SERVER_NETAPI_INFO_HPP
#define XASH_ENGINE_SERVER_NETAPI_INFO_HPP

#include <cstddef>

namespace xash
{
namespace engine
{
namespace server
{

struct LegacyServerInfo
{
	int requestProtocol;
	int protocolVersion;
	const char *hostname;
	const char *mapName;
	bool deathmatch;
	bool teamplay;
	bool coop;
	int playerCount;
	int maxPlayers;
	const char *gameFolder;
	bool passwordProtected;
};

struct NetApiRuleInfo
{
	const char *name;
	const char *value;
	bool isProtected;
};

struct NetApiPlayerInfo
{
	const char *name;
	int frags;
	float time;
};

struct NetApiDetailsInfo
{
	const char *hostname;
	const char *gameFolder;
	int currentPlayers;
	int maxPlayers;
	const char *mapName;
};

bool BuildLegacyServerInfoString(
	char *out,
	std::size_t capacity,
	const LegacyServerInfo &info);

bool BuildNetApiProtocolError(char *out, std::size_t capacity);
bool BuildNetApiUndefinedError(char *out, std::size_t capacity);
bool BuildNetApiForbiddenError(char *out, std::size_t capacity);
bool BuildNetApiPing(char *out, std::size_t capacity);

bool BeginNetApiRules(char *out, std::size_t capacity);
bool AppendNetApiRule(
	char *out,
	std::size_t capacity,
	const NetApiRuleInfo &rule);
bool FinishNetApiRules(char *out, std::size_t capacity, int count);

bool BeginNetApiPlayers(char *out, std::size_t capacity);
bool AppendNetApiPlayer(
	char *out,
	std::size_t capacity,
	int index,
	const NetApiPlayerInfo &player);
bool FinishNetApiPlayers(char *out, std::size_t capacity, int count);

bool BuildNetApiDetails(
	char *out,
	std::size_t capacity,
	const NetApiDetailsInfo &details);

}
}
}

#endif
