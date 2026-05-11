#include "engine/server/client/source_query.hpp"

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

}
}
}
