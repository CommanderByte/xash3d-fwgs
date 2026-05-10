#ifndef XASH_ENGINE_SERVER_SOURCE_QUERY_HPP
#define XASH_ENGINE_SERVER_SOURCE_QUERY_HPP

#include <cstddef>
#include <cstdint>

namespace xash
{
namespace engine
{
namespace server
{

constexpr uint8_t kSourceQueryInfoResponse = 'I';
constexpr uint8_t kSourceQueryRulesResponse = 'E';
constexpr uint8_t kSourceQueryPlayersResponse = 'D';
constexpr uint32_t kSourceQueryConnectionlessHeader = 0xFFFFFFFFU;

struct SourceQueryDetails
{
	int protocolVersion;
	const char *hostname;
	const char *mapName;
	const char *gameFolder;
	const char *gameDescription;
	int appId;
	int playerCount;
	int maxPlayers;
	int botCount;
	char serverType;
	char platform;
	bool passwordProtected;
	int secure;
	const char *version;
};

struct SourceQueryRule
{
	const char *name;
	const char *value;
	bool isProtected;
};

struct SourceQueryPlayer
{
	unsigned int index;
	const char *name;
	int frags;
	float duration;
};

const char *SourceQueryProtectedValue(const char *value);
bool SourceQueryAllowsPlayerList(bool exposePlayerList, bool passwordProtected);

std::size_t BuildSourceQueryDetails(
	const SourceQueryDetails &details,
	void *buffer,
	std::size_t capacity);

std::size_t BuildSourceQueryRules(
	const SourceQueryRule *rules,
	std::size_t count,
	void *buffer,
	std::size_t capacity);

std::size_t BeginSourceQueryRules(void *buffer, std::size_t capacity);
std::size_t AppendSourceQueryRule(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	const SourceQueryRule &rule);
std::size_t FinishSourceQueryRules(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	std::size_t count);

std::size_t BuildSourceQueryPlayers(
	const SourceQueryPlayer *players,
	std::size_t count,
	void *buffer,
	std::size_t capacity);

std::size_t BeginSourceQueryPlayers(void *buffer, std::size_t capacity);
std::size_t AppendSourceQueryPlayer(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	const SourceQueryPlayer &player);
std::size_t FinishSourceQueryPlayers(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	std::size_t count);

}
}
}

#endif
