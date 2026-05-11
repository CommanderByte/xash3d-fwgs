#include "engine/server/save/save_restore_format.hpp"
#include "engine/server/save/save_restore_values.hpp"
#include "engine/server/save/save_restore_runtime.hpp"

#include <algorithm>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

static_assert(
	sizeof(short) == kSaveRestorePackedShortBytes,
	"Legacy save/restore viewentity fields assume a 16-bit short");

bool CanRead(std::size_t size, std::size_t offset, std::size_t count)
{
	return offset <= size && count <= size - offset;
}

std::uint16_t ReadU16(const std::uint8_t *data, std::size_t offset)
{
	return static_cast<std::uint16_t>(data[offset]) |
		(static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t ReadU32(const std::uint8_t *data, std::size_t offset)
{
	return static_cast<std::uint32_t>(data[offset]) |
		(static_cast<std::uint32_t>(data[offset + 1]) << 8) |
		(static_cast<std::uint32_t>(data[offset + 2]) << 16) |
		(static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

int ReadI32(const std::uint8_t *data, std::size_t offset)
{
	return static_cast<int>(ReadU32(data, offset));
}

SaveRestoreBlockHeader MakeHeaderStatus(SaveRestoreParseStatus status)
{
	SaveRestoreBlockHeader header = {};
	header.status = status;
	return header;
}

bool ValidTokenIndex(
	const std::vector<std::string> &tokens,
	std::uint16_t index)
{
	return index < tokens.size() && !tokens[index].empty();
}

std::string ReadFixedName(const std::uint8_t *data)
{
	const char *begin = reinterpret_cast<const char *>(data);
	const char *end = std::find(begin, begin + kSaveRestoreBundledNameBytes, '\0');
	return std::string(begin, end);
}

}

SaveRestoreBlockHeader ParseSaveRestoreBlockHeader(
	const std::uint8_t *data,
	std::size_t size)
{
	if (!data || size < 8)
		return MakeHeaderStatus(SaveRestoreParseStatus::Truncated);

	SaveRestoreBlockHeader header = {};
	header.magic = ReadU32(data, 0);
	header.version = ReadI32(data, 4);
	header.kind = SaveRestoreBlockKind::Unknown;

	if (header.magic == kSaveRestoreLevelMagic)
	{
		if (header.version != kSaveRestoreGameVersion)
			return MakeHeaderStatus(SaveRestoreParseStatus::UnsupportedVersion);

		if (size < 24)
			return MakeHeaderStatus(SaveRestoreParseStatus::Truncated);

		header.kind = SaveRestoreBlockKind::Level;
		header.headerBytes = 24;
		header.dataSize = ReadI32(data, 8);
		header.tableCount = ReadI32(data, 12);
		header.tokenCount = ReadI32(data, 16);
		header.tokenSize = ReadI32(data, 20);
	}
	else if (header.magic == kSaveRestoreGameMagic)
	{
		if (header.version != kSaveRestoreGameVersion &&
			header.version != kSaveRestoreClientVersion)
		{
			return MakeHeaderStatus(SaveRestoreParseStatus::UnsupportedVersion);
		}

		if (size < 20)
			return MakeHeaderStatus(SaveRestoreParseStatus::Truncated);

		header.kind = header.version == kSaveRestoreClientVersion
			? SaveRestoreBlockKind::Client
			: SaveRestoreBlockKind::Game;
		header.headerBytes = 20;
		header.dataSize = ReadI32(data, 8);
		header.tableCount = 0;
		header.tokenCount = ReadI32(data, 12);
		header.tokenSize = ReadI32(data, 16);
	}
	else
	{
		return MakeHeaderStatus(SaveRestoreParseStatus::InvalidMagic);
	}

	if (header.dataSize < 0 ||
		header.tableCount < 0 ||
		header.tokenCount < 0 ||
		header.tokenCount > kSaveRestoreHashStrings ||
		header.tokenSize < 0 ||
		header.tokenSize > kSaveRestoreHeapSize ||
		header.dataSize > kSaveRestoreHeapSize)
	{
		return MakeHeaderStatus(SaveRestoreParseStatus::InvalidCount);
	}

	header.tokenOffset = header.headerBytes;
	header.payloadOffset = header.tokenOffset +
		static_cast<std::size_t>(header.tokenSize);
	header.payloadEnd = header.payloadOffset +
		static_cast<std::size_t>(header.dataSize);

	if (header.payloadOffset < header.tokenOffset ||
		header.payloadEnd < header.payloadOffset ||
		header.payloadEnd > size)
	{
		return MakeHeaderStatus(SaveRestoreParseStatus::Truncated);
	}

	header.status = SaveRestoreParseStatus::Ok;
	return header;
}

SaveRestoreTokenTable ParseSaveRestoreTokenTable(
	const std::uint8_t *data,
	std::size_t size,
	const SaveRestoreBlockHeader &header)
{
	SaveRestoreTokenTable table = {};

	if (header.status != SaveRestoreParseStatus::Ok)
	{
		table.status = header.status;
		return table;
	}

	if (!CanRead(size, header.tokenOffset, header.tokenSize))
	{
		table.status = SaveRestoreParseStatus::Truncated;
		return table;
	}

	const std::uint8_t *tokens = data + header.tokenOffset;
	std::size_t offset = 0;

	for (int i = 0; i < header.tokenCount; ++i)
	{
		std::size_t start = offset;
		while (offset < static_cast<std::size_t>(header.tokenSize) &&
			tokens[offset] != 0)
		{
			++offset;
		}

		if (offset >= static_cast<std::size_t>(header.tokenSize))
		{
			table.status = SaveRestoreParseStatus::MissingTerminator;
			return table;
		}

		table.tokens.push_back(std::string(
			reinterpret_cast<const char *>(tokens + start),
			offset - start));
		++offset;
	}

	table.consumedBytes = offset;
	table.rebasedPayloadOffset = header.tokenOffset + offset;
	table.status = SaveRestoreParseStatus::Ok;
	return table;
}

SaveRestoreParseStatus ParseSaveRestoreFieldSection(
	const std::uint8_t *payload,
	std::size_t payloadSize,
	const std::vector<std::string> &tokens,
	std::size_t *offset,
	SaveRestoreFieldSection *section)
{
	if (!payload || !offset || !section)
		return SaveRestoreParseStatus::Truncated;

	std::size_t cursor = *offset;
	if (!CanRead(payloadSize, cursor, 4))
		return SaveRestoreParseStatus::Truncated;

	const std::uint16_t headerSize = ReadU16(payload, cursor);
	cursor += 2;
	const std::uint16_t sectionNameIndex = ReadU16(payload, cursor);
	cursor += 2;

	if (!ValidTokenIndex(tokens, sectionNameIndex))
		return SaveRestoreParseStatus::InvalidTokenIndex;

	if (headerSize == 0 || !CanRead(payloadSize, cursor, headerSize))
		return SaveRestoreParseStatus::InvalidFieldSection;

	section->name = tokens[sectionNameIndex];
	section->headerData.assign(payload + cursor, payload + cursor + headerSize);
	const std::uint8_t fieldCount = payload[cursor];
	cursor += headerSize;

	section->fields.clear();
	for (std::uint8_t i = 0; i < fieldCount; ++i)
	{
		if (!CanRead(payloadSize, cursor, 4))
			return SaveRestoreParseStatus::Truncated;

		const std::uint16_t fieldSize = ReadU16(payload, cursor);
		cursor += 2;
		const std::uint16_t fieldNameIndex = ReadU16(payload, cursor);
		cursor += 2;

		if (!ValidTokenIndex(tokens, fieldNameIndex))
			return SaveRestoreParseStatus::InvalidTokenIndex;

		if (!CanRead(payloadSize, cursor, fieldSize))
			return SaveRestoreParseStatus::Truncated;

		SaveRestoreField field = {};
		field.name = tokens[fieldNameIndex];
		field.data.assign(payload + cursor, payload + cursor + fieldSize);
		cursor += fieldSize;
		section->fields.push_back(field);
	}

	*offset = cursor;
	return SaveRestoreParseStatus::Ok;
}

SaveRestoreBundledFile ParseSaveRestoreBundledFile(
	const std::uint8_t *data,
	std::size_t size,
	std::size_t offset)
{
	SaveRestoreBundledFile file = {};

	if (!data || !CanRead(size, offset, kSaveRestoreBundledNameBytes + 4))
	{
		file.status = SaveRestoreParseStatus::Truncated;
		return file;
	}

	file.name = ReadFixedName(data + offset);
	offset += kSaveRestoreBundledNameBytes;
	file.size = ReadI32(data, offset);
	offset += 4;

	if (file.size < 0)
	{
		file.status = SaveRestoreParseStatus::InvalidCount;
		return file;
	}

	file.dataOffset = offset;
	file.nextOffset = offset + static_cast<std::size_t>(file.size);

	if (file.nextOffset < file.dataOffset || file.nextOffset > size)
	{
		file.status = SaveRestoreParseStatus::Truncated;
		return file;
	}

	file.status = SaveRestoreParseStatus::Ok;
	return file;
}

SaveRestoreEntityPatch ParseSaveRestoreEntityPatch(
	const std::uint8_t *data,
	std::size_t size,
	int tableCount)
{
	SaveRestoreEntityPatch patch = {};

	if (!data || !CanRead(size, 0, 4))
	{
		patch.status = SaveRestoreParseStatus::Truncated;
		return patch;
	}

	patch.patchCount = ReadI32(data, 0);
	if (patch.patchCount < 0 || tableCount < 0)
	{
		patch.status = SaveRestoreParseStatus::InvalidCount;
		return patch;
	}

	const std::size_t bytesNeeded =
		4 + static_cast<std::size_t>(patch.patchCount) * 4;
	if (bytesNeeded < 4 || !CanRead(size, 0, bytesNeeded))
	{
		patch.status = SaveRestoreParseStatus::Truncated;
		return patch;
	}

	std::size_t offset = 4;
	for (int i = 0; i < patch.patchCount; ++i)
	{
		const int entityIndex = ReadI32(data, offset);
		offset += 4;

		if (entityIndex < 0 || entityIndex >= tableCount)
		{
			patch.invalidEntityIndexes.push_back(entityIndex);
			continue;
		}

		patch.removedEntityIndexes.push_back(entityIndex);
	}

	patch.consumedBytes = offset;
	patch.status = patch.invalidEntityIndexes.empty()
		? SaveRestoreParseStatus::Ok
		: SaveRestoreParseStatus::InvalidEntityPatchIndex;
	return patch;
}

SaveRestorePackedShort ParseSaveRestorePackedShort(
	const std::uint8_t *data,
	std::size_t size)
{
	SaveRestorePackedShort value = {};

	if (!data || !CanRead(size, 0, kSaveRestorePackedShortBytes))
	{
		value.status = SaveRestoreParseStatus::Truncated;
		return value;
	}

	const std::uint16_t raw = ReadU16(data, 0);
	value.value = raw <= 0x7FFF
		? static_cast<std::int16_t>(raw)
		: static_cast<std::int16_t>(static_cast<int>(raw) - 0x10000);
	value.status = SaveRestoreParseStatus::Ok;
	return value;
}

SaveAdmissionPlan MakeSaveAdmissionPlan(
	SaveAdmissionDecision decision,
	const char *legacyConsoleMessage)
{
	SaveAdmissionPlan plan = {};
	plan.decision = decision;
	plan.allowed = decision == SaveAdmissionDecision::Allow;
	plan.legacyConsoleMessage = legacyConsoleMessage;
	return plan;
}

const char *NonNullText(const char *text)
{
	return text ? text : "";
}

SaveAdmissionPlan BuildSaveAdmissionPlan(
	const SaveAdmissionSnapshot &snapshot)
{
	if (!snapshot.serverInitialized || !snapshot.serverActive)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::NotPlayingLocalGame,
			"Not playing a local game.\n");
	}

	if (snapshot.backgroundMap || snapshot.creditsActive)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::BackgroundOrCredits,
			nullptr);
	}

	if (snapshot.physicsCallbackAvailable && !snapshot.physicsAllowsSave)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::PhysicsVeto,
			"Savegame is not allowed.\n");
	}

	if (!snapshot.clientActive)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::InactiveClient,
			"Can't save if not active.\n");
	}

	if (snapshot.intermission)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::Intermission,
			"Can't save during intermission.\n");
	}

	if (snapshot.maxClients != 1)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::Multiplayer,
			"Can't save multiplayer games.\n");
	}

	if (!snapshot.spawnedClientAvailable)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::MissingClient,
			"Can't savegame without a client!\n");
	}

	if (!snapshot.playerEdictAvailable)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::MissingPlayer,
			"Can't savegame without a player!\n");
	}

	if (snapshot.playerDeadFlag || snapshot.playerHealth <= 0.0f)
	{
		return MakeSaveAdmissionPlan(
			SaveAdmissionDecision::DeadPlayer,
			"Can't savegame with a dead player\n");
	}

	return MakeSaveAdmissionPlan(SaveAdmissionDecision::Allow, nullptr);
}

const char *SaveAdmissionDecisionName(SaveAdmissionDecision decision)
{
	switch (decision)
	{
	case SaveAdmissionDecision::Allow:
		return "allow";
	case SaveAdmissionDecision::NotPlayingLocalGame:
		return "not_playing_local_game";
	case SaveAdmissionDecision::BackgroundOrCredits:
		return "background_or_credits";
	case SaveAdmissionDecision::PhysicsVeto:
		return "physics_veto";
	case SaveAdmissionDecision::InactiveClient:
		return "inactive_client";
	case SaveAdmissionDecision::Intermission:
		return "intermission";
	case SaveAdmissionDecision::Multiplayer:
		return "multiplayer";
	case SaveAdmissionDecision::MissingClient:
		return "missing_client";
	case SaveAdmissionDecision::MissingPlayer:
		return "missing_player";
	case SaveAdmissionDecision::DeadPlayer:
		return "dead_player";
	}

	return "unknown";
}

SaveGameCommentHeaderPlan BuildSaveGameCommentHeaderPlan(
	bool fileOpened,
	std::uint32_t magic,
	int version)
{
	SaveGameCommentHeaderPlan plan = {};

	if (!fileOpened)
	{
		plan.decision = SaveGameCommentHeaderDecision::ClearMissingFile;
		plan.legacyCommentText = "";
		return plan;
	}

	if (magic != kSaveRestoreGameMagic)
	{
		plan.decision = SaveGameCommentHeaderDecision::CorruptedHeader;
		plan.legacyCommentText = "<corrupted>";
		return plan;
	}

	if (version == kSaveRestoreOldUnsupportedVersion)
	{
		plan.decision = SaveGameCommentHeaderDecision::OldUnsupportedVersion;
		plan.legacyCommentText = "<old version Xash3D FWGS unsupported>";
		return plan;
	}

	if (version < kSaveRestoreGameVersion)
	{
		plan.decision = SaveGameCommentHeaderDecision::OldVersion;
		plan.legacyCommentText = "<old version>";
		return plan;
	}

	if (version > kSaveRestoreGameVersion)
	{
		plan.decision = SaveGameCommentHeaderDecision::InvalidVersion;
		plan.legacyCommentText = "<invalid version>";
		return plan;
	}

	plan.decision = SaveGameCommentHeaderDecision::ReadFields;
	plan.shouldReadFields = true;
	return plan;
}

const char *SaveGameCommentHeaderDecisionName(
	SaveGameCommentHeaderDecision decision)
{
	switch (decision)
	{
	case SaveGameCommentHeaderDecision::ReadFields:
		return "read_fields";
	case SaveGameCommentHeaderDecision::ClearMissingFile:
		return "clear_missing_file";
	case SaveGameCommentHeaderDecision::CorruptedHeader:
		return "corrupted_header";
	case SaveGameCommentHeaderDecision::OldUnsupportedVersion:
		return "old_unsupported_version";
	case SaveGameCommentHeaderDecision::OldVersion:
		return "old_version";
	case SaveGameCommentHeaderDecision::InvalidVersion:
		return "invalid_version";
	}

	return "unknown";
}

SaveCommentFallbackPlan BuildSaveCommentFallbackPlan(
	const SaveCommentFallbackSnapshot &snapshot)
{
	SaveCommentFallbackPlan plan = {};

	if (snapshot.gameDllCommentAvailable)
	{
		plan.source = SaveCommentSource::GameDllOverride;
		plan.sourceText = NonNullText(snapshot.gameDllComment);
	}
	else if (snapshot.titleAlias && snapshot.titleAlias[0])
	{
		plan.source = SaveCommentSource::TitleAlias;
		plan.sourceText = snapshot.titleAlias;
	}
	else if (snapshot.worldMessageAvailable)
	{
		plan.source = SaveCommentSource::WorldMessage;
		plan.sourceText = NonNullText(snapshot.worldMessage);
	}
	else
	{
		plan.source = SaveCommentSource::MapName;
		plan.sourceText = NonNullText(snapshot.mapName);
	}

	plan.elapsedMinutes = static_cast<int>(snapshot.elapsedSeconds / 60.0);
	plan.elapsedSecondRemainder =
		static_cast<int>(
			snapshot.elapsedSeconds -
			static_cast<double>(plan.elapsedMinutes) * 60.0);
	return plan;
}

const char *SaveCommentSourceName(SaveCommentSource source)
{
	switch (source)
	{
	case SaveCommentSource::GameDllOverride:
		return "game_dll_override";
	case SaveCommentSource::TitleAlias:
		return "title_alias";
	case SaveCommentSource::WorldMessage:
		return "world_message";
	case SaveCommentSource::MapName:
		return "map_name";
	}

	return "unknown";
}

SaveArchiveManifest ParseSaveArchiveManifest(
	const std::uint8_t *data,
	std::size_t size,
	std::size_t offset,
	int fileCount)
{
	SaveArchiveManifest manifest = {};

	if (fileCount < 0)
	{
		manifest.status = SaveRestoreParseStatus::InvalidCount;
		return manifest;
	}

	std::size_t cursor = offset;
	for (int i = 0; i < fileCount; ++i)
	{
		const SaveRestoreBundledFile bundled =
			ParseSaveRestoreBundledFile(data, size, cursor);
		if (bundled.status != SaveRestoreParseStatus::Ok)
		{
			manifest.status = bundled.status;
			manifest.consumedBytes = cursor - offset;
			return manifest;
		}

		SaveArchiveManifestEntry entry = {};
		entry.name = bundled.name;
		entry.size = bundled.size;
		entry.dataOffset = bundled.dataOffset;
		entry.nextOffset = bundled.nextOffset;
		manifest.entries.push_back(entry);
		cursor = bundled.nextOffset;
	}

	manifest.status = SaveRestoreParseStatus::Ok;
	manifest.consumedBytes = cursor - offset;
	return manifest;
}

SaveEntityPatchWritePlan BuildSaveEntityPatchWritePlan(
	const SaveEntityTableRowSnapshot *rows,
	std::size_t rowCount)
{
	SaveEntityPatchWritePlan plan = {};

	if (!rows)
		return plan;

	for (std::size_t i = 0; i < rowCount; ++i)
	{
		if (rows[i].removed)
			plan.removedEntityIndexes.push_back(static_cast<int>(i));
	}

	return plan;
}

SaveEntityPatchApplyPlan BuildSaveEntityPatchApplyPlan(
	const std::uint8_t *data,
	std::size_t size,
	int tableCount)
{
	const SaveRestoreEntityPatch patch =
		ParseSaveRestoreEntityPatch(data, size, tableCount);

	SaveEntityPatchApplyPlan plan = {};
	plan.status = patch.status;
	plan.markRemovedEntityIndexes = patch.removedEntityIndexes;
	plan.invalidEntityIndexes = patch.invalidEntityIndexes;
	return plan;
}

}
}
}
