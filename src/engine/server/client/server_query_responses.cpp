#include "engine/server/client/source_query.hpp"
#include "engine/server/client/netapi_info.hpp"

#include "engine/info_string.hpp"

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

constexpr std::size_t kRulesCountOffset = 5;
constexpr std::size_t kPlayersCountOffset = 5;

char LowerAscii(char value)
{
	if (value >= 'A' && value <= 'Z')
		return static_cast<char>(value - 'A' + 'a');

	return value;
}

bool EqualsIgnoreCase(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs)
		return lhs == rhs;

	while (*lhs && *rhs)
	{
		if (LowerAscii(*lhs) != LowerAscii(*rhs))
			return false;

		++lhs;
		++rhs;
	}

	return *lhs == *rhs;
}

bool StringEmpty(const char *value)
{
	return !value || value[0] == '\0';
}

uint8_t LegacyByte(int value)
{
	return static_cast<uint8_t>(value);
}

uint16_t LegacyShort(int value)
{
	return static_cast<uint16_t>(value);
}

class SourceQueryWriter
{
public:
	SourceQueryWriter(void *buffer, std::size_t capacity, std::size_t offset = 0) :
		buffer_(static_cast<uint8_t *>(buffer)),
		capacity_(capacity),
		offset_(offset)
	{
		if (!buffer || offset > capacity)
			overflowed_ = true;
	}

	bool writeByte(uint8_t value)
	{
		if (!ensure(1))
			return false;

		buffer_[offset_++] = value;
		return true;
	}

	bool writeShort(uint16_t value)
	{
		if (!ensure(2))
			return false;

		buffer_[offset_++] = static_cast<uint8_t>(value & 0xffU);
		buffer_[offset_++] = static_cast<uint8_t>((value >> 8) & 0xffU);
		return true;
	}

	bool writeLong(int32_t value)
	{
		return writeDword(static_cast<uint32_t>(value));
	}

	bool writeDword(uint32_t value)
	{
		if (!ensure(4))
			return false;

		buffer_[offset_++] = static_cast<uint8_t>(value & 0xffU);
		buffer_[offset_++] = static_cast<uint8_t>((value >> 8) & 0xffU);
		buffer_[offset_++] = static_cast<uint8_t>((value >> 16) & 0xffU);
		buffer_[offset_++] = static_cast<uint8_t>((value >> 24) & 0xffU);
		return true;
	}

	bool writeFloat(float value)
	{
		uint32_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		return writeDword(bits);
	}

	bool writeString(const char *value)
	{
		if (!value)
			value = "";

		const std::size_t length = std::strlen(value) + 1;

		if (!ensure(length))
			return false;

		std::memcpy(buffer_ + offset_, value, length);
		offset_ += length;
		return true;
	}

	bool patchByte(std::size_t offset, uint8_t value)
	{
		if (!buffer_ || offset >= capacity_)
		{
			overflowed_ = true;
			return false;
		}

		buffer_[offset] = value;
		return true;
	}

	bool patchShort(std::size_t offset, uint16_t value)
	{
		if (!buffer_ || offset + 1 >= capacity_)
		{
			overflowed_ = true;
			return false;
		}

		buffer_[offset] = static_cast<uint8_t>(value & 0xffU);
		buffer_[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xffU);
		return true;
	}

	std::size_t size() const
	{
		if (overflowed_)
			return 0;

		return offset_;
	}

private:
	bool ensure(std::size_t length)
	{
		if (overflowed_ || !buffer_ || length > capacity_ || offset_ > capacity_ - length)
		{
			overflowed_ = true;
			return false;
		}

		return true;
	}

	uint8_t *buffer_ = nullptr;
	std::size_t capacity_ = 0;
	std::size_t offset_ = 0;
	bool overflowed_ = false;
};

void WriteQueryHeader(SourceQueryWriter &writer, uint8_t responseType)
{
	writer.writeDword(kSourceQueryConnectionlessHeader);
	writer.writeByte(responseType);
}

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

const char *SourceQueryProtectedValue(const char *value)
{
	if (StringEmpty(value) || EqualsIgnoreCase(value, "none"))
		return "0";

	return "1";
}

bool SourceQueryAllowsPlayerList(bool exposePlayerList, bool passwordProtected)
{
	return exposePlayerList && !passwordProtected;
}

std::size_t BuildSourceQueryDetails(
	const SourceQueryDetails &details,
	void *buffer,
	std::size_t capacity)
{
	SourceQueryWriter writer(buffer, capacity);

	WriteQueryHeader(writer, kSourceQueryInfoResponse);
	writer.writeByte(LegacyByte(details.protocolVersion));
	writer.writeString(details.hostname);
	writer.writeString(details.mapName);
	writer.writeString(details.gameFolder);
	writer.writeString(details.gameDescription);
	writer.writeShort(LegacyShort(details.appId));
	writer.writeByte(LegacyByte(details.playerCount));
	writer.writeByte(LegacyByte(details.maxPlayers));
	writer.writeByte(LegacyByte(details.botCount));
	writer.writeByte(static_cast<uint8_t>(details.serverType));
	writer.writeByte(static_cast<uint8_t>(details.platform));
	writer.writeByte(details.passwordProtected ? 1 : 0);
	writer.writeByte(LegacyByte(details.secure));
	writer.writeString(details.version);

	return writer.size();
}

std::size_t BuildSourceQueryRules(
	const SourceQueryRule *rules,
	std::size_t count,
	void *buffer,
	std::size_t capacity)
{
	std::size_t offset = BeginSourceQueryRules(buffer, capacity);

	if (!offset)
		return 0;

	for (std::size_t i = 0; i < count; ++i)
	{
		offset = AppendSourceQueryRule(buffer, capacity, offset, rules[i]);

		if (!offset)
			return 0;
	}

	return FinishSourceQueryRules(buffer, capacity, offset, count);
}

std::size_t BeginSourceQueryRules(void *buffer, std::size_t capacity)
{
	SourceQueryWriter writer(buffer, capacity);

	WriteQueryHeader(writer, kSourceQueryRulesResponse);
	writer.writeShort(0);

	return writer.size();
}

std::size_t AppendSourceQueryRule(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	const SourceQueryRule &rule)
{
	SourceQueryWriter writer(buffer, capacity, offset);

	writer.writeString(rule.name);
	writer.writeString(rule.isProtected ? SourceQueryProtectedValue(rule.value) : rule.value);

	return writer.size();
}

std::size_t FinishSourceQueryRules(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	std::size_t count)
{
	if (count == 0)
		return 0;

	SourceQueryWriter writer(buffer, capacity, offset);
	writer.patchShort(kRulesCountOffset, LegacyShort(static_cast<int>(count)));

	return writer.size();
}

std::size_t BuildSourceQueryPlayers(
	const SourceQueryPlayer *players,
	std::size_t count,
	void *buffer,
	std::size_t capacity)
{
	std::size_t offset = BeginSourceQueryPlayers(buffer, capacity);

	if (!offset)
		return 0;

	for (std::size_t i = 0; i < count; ++i)
	{
		offset = AppendSourceQueryPlayer(buffer, capacity, offset, players[i]);

		if (!offset)
			return 0;
	}

	return FinishSourceQueryPlayers(buffer, capacity, offset, count);
}

std::size_t BeginSourceQueryPlayers(void *buffer, std::size_t capacity)
{
	SourceQueryWriter writer(buffer, capacity);

	WriteQueryHeader(writer, kSourceQueryPlayersResponse);
	writer.writeByte(0);

	return writer.size();
}

std::size_t AppendSourceQueryPlayer(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	const SourceQueryPlayer &player)
{
	SourceQueryWriter writer(buffer, capacity, offset);

	writer.writeByte(LegacyByte(static_cast<int>(player.index)));
	writer.writeString(player.name);
	writer.writeLong(player.frags);
	writer.writeFloat(player.duration);

	return writer.size();
}

std::size_t FinishSourceQueryPlayers(
	void *buffer,
	std::size_t capacity,
	std::size_t offset,
	std::size_t count)
{
	if (count == 0)
		return 0;

	SourceQueryWriter writer(buffer, capacity, offset);
	writer.patchByte(kPlayersCountOffset, LegacyByte(static_cast<int>(count)));

	return writer.size();
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
