#ifndef XASH_ENGINE_SERVER_SAVE_RESTORE_FORMAT_HPP
#define XASH_ENGINE_SERVER_SAVE_RESTORE_FORMAT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xash
{
namespace engine
{
namespace server
{

constexpr std::uint32_t kSaveRestoreLevelMagic = 0x564C4156u;
constexpr std::uint32_t kSaveRestoreGameMagic = 0x5641534Au;
constexpr int kSaveRestoreGameVersion = 0x0071;
constexpr int kSaveRestoreClientVersion = 0x0067;
constexpr int kSaveRestoreHeapSize = 0x400000;
constexpr int kSaveRestoreHashStrings = 0x0FFF;
constexpr int kSaveRestoreBundledNameBytes = 260;
constexpr std::size_t kSaveRestorePackedShortBytes = 2;

enum class SaveRestoreBlockKind
{
	Unknown = 0,
	Level = 1,
	Game = 2,
	Client = 3
};

enum class SaveRestoreParseStatus
{
	Ok = 0,
	Truncated = 1,
	InvalidMagic = 2,
	UnsupportedVersion = 3,
	InvalidCount = 4,
	MissingTerminator = 5,
	InvalidTokenIndex = 6,
	InvalidFieldSection = 7,
	InvalidEntityPatchIndex = 8
};

struct SaveRestoreBlockHeader
{
	SaveRestoreParseStatus status;
	SaveRestoreBlockKind kind;
	std::uint32_t magic;
	int version;
	int dataSize;
	int tableCount;
	int tokenCount;
	int tokenSize;
	std::size_t headerBytes;
	std::size_t tokenOffset;
	std::size_t payloadOffset;
	std::size_t payloadEnd;
};

struct SaveRestoreTokenTable
{
	SaveRestoreParseStatus status;
	std::vector<std::string> tokens;
	std::size_t consumedBytes;
	std::size_t rebasedPayloadOffset;
};

struct SaveRestoreField
{
	std::string name;
	std::vector<std::uint8_t> data;
};

struct SaveRestoreFieldSection
{
	std::string name;
	std::vector<std::uint8_t> headerData;
	std::vector<SaveRestoreField> fields;
};

struct SaveRestoreBundledFile
{
	SaveRestoreParseStatus status;
	std::string name;
	int size;
	std::size_t dataOffset;
	std::size_t nextOffset;
};

struct SaveRestoreEntityPatch
{
	SaveRestoreParseStatus status;
	int patchCount;
	std::vector<int> removedEntityIndexes;
	std::vector<int> invalidEntityIndexes;
	std::size_t consumedBytes;
};

struct SaveRestorePackedShort
{
	SaveRestoreParseStatus status;
	std::int16_t value;
};

SaveRestoreBlockHeader ParseSaveRestoreBlockHeader(
	const std::uint8_t *data,
	std::size_t size);

SaveRestoreTokenTable ParseSaveRestoreTokenTable(
	const std::uint8_t *data,
	std::size_t size,
	const SaveRestoreBlockHeader &header);

SaveRestoreParseStatus ParseSaveRestoreFieldSection(
	const std::uint8_t *payload,
	std::size_t payloadSize,
	const std::vector<std::string> &tokens,
	std::size_t *offset,
	SaveRestoreFieldSection *section);

SaveRestoreBundledFile ParseSaveRestoreBundledFile(
	const std::uint8_t *data,
	std::size_t size,
	std::size_t offset);

SaveRestoreEntityPatch ParseSaveRestoreEntityPatch(
	const std::uint8_t *data,
	std::size_t size,
	int tableCount);

SaveRestorePackedShort ParseSaveRestorePackedShort(
	const std::uint8_t *data,
	std::size_t size);

}
}
}

#endif
