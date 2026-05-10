#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/server/save_restore_format.hpp"

using namespace xash::engine::server;

namespace
{

using Bytes = std::vector<std::uint8_t>;

static void AppendU16(Bytes &out, std::uint16_t value)
{
	out.push_back(static_cast<std::uint8_t>(value & 0xFF));
	out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

static void AppendI32(Bytes &out, int value)
{
	const std::uint32_t bits = static_cast<std::uint32_t>(value);
	out.push_back(static_cast<std::uint8_t>(bits & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xFF));
}

static void AppendU32(Bytes &out, std::uint32_t value)
{
	AppendI32(out, static_cast<int>(value));
}

static void AppendStringToken(Bytes &out, const char *value)
{
	while (*value)
		out.push_back(static_cast<std::uint8_t>(*value++));
	out.push_back(0);
}

static void AppendField(Bytes &out, std::uint16_t nameToken, const Bytes &data)
{
	AppendU16(out, static_cast<std::uint16_t>(data.size()));
	AppendU16(out, nameToken);
	for (std::size_t i = 0; i < data.size(); ++i)
		out.push_back(data[i]);
}

static Bytes I32Data(int value)
{
	Bytes out;
	AppendI32(out, value);
	return out;
}

static Bytes TextData(const char *value)
{
	Bytes out;
	while (*value)
		out.push_back(static_cast<std::uint8_t>(*value++));
	out.push_back(0);
	return out;
}

static void AppendSection(
	Bytes &out,
	std::uint16_t sectionToken,
	const std::vector<std::pair<std::uint16_t, Bytes>> &fields)
{
	AppendU16(out, 1);
	AppendU16(out, sectionToken);
	out.push_back(static_cast<std::uint8_t>(fields.size()));

	for (std::size_t i = 0; i < fields.size(); ++i)
		AppendField(out, fields[i].first, fields[i].second);
}

static Bytes TokenTable(const std::vector<const char *> &tokens)
{
	Bytes out;
	for (std::size_t i = 0; i < tokens.size(); ++i)
		AppendStringToken(out, tokens[i]);
	return out;
}

static Bytes BuildLevelFixture(
	const Bytes &tokenData,
	int tokenCount,
	const Bytes &payload,
	int tableCount)
{
	Bytes out;
	AppendU32(out, kSaveRestoreLevelMagic);
	AppendI32(out, kSaveRestoreGameVersion);
	AppendI32(out, static_cast<int>(payload.size()));
	AppendI32(out, tableCount);
	AppendI32(out, tokenCount);
	AppendI32(out, static_cast<int>(tokenData.size()));
	for (std::size_t i = 0; i < tokenData.size(); ++i)
		out.push_back(tokenData[i]);
	for (std::size_t i = 0; i < payload.size(); ++i)
		out.push_back(payload[i]);
	return out;
}

static Bytes BuildGameFixture(
	int version,
	int tokenCount,
	const Bytes &tokenData,
	const Bytes &payload)
{
	Bytes out;
	AppendU32(out, kSaveRestoreGameMagic);
	AppendI32(out, version);
	AppendI32(out, static_cast<int>(payload.size()));
	AppendI32(out, tokenCount);
	AppendI32(out, static_cast<int>(tokenData.size()));
	for (std::size_t i = 0; i < tokenData.size(); ++i)
		out.push_back(tokenData[i]);
	for (std::size_t i = 0; i < payload.size(); ++i)
		out.push_back(payload[i]);
	return out;
}

static bool TestLevelHeaderTokenTableAndSections()
{
	enum Token
	{
		TEmpty,
		TEntityTable,
		TSaveHeader,
		TLightStyle,
		TId,
		TClassname,
		TEntityCount,
		TConnectionCount,
		TLightStyleCount,
		TTime,
		TIndex,
		TStyle,
	};

	const Bytes tokens = TokenTable({
		"",
		"ETABLE",
		"Save Header",
		"LIGHTSTYLE",
		"id",
		"classname",
		"entityCount",
		"connectionCount",
		"lightStyleCount",
		"time",
		"index",
		"style",
	});

	Bytes payload;
	AppendSection(payload, TEntityTable, {
		{ TId, I32Data(1) },
		{ TClassname, TextData("worldspawn") },
	});
	AppendSection(payload, TSaveHeader, {
		{ TEntityCount, I32Data(1) },
		{ TConnectionCount, I32Data(0) },
		{ TLightStyleCount, I32Data(1) },
		{ TTime, I32Data(42) },
	});
	AppendSection(payload, TLightStyle, {
		{ TIndex, I32Data(3) },
		{ TStyle, TextData("mmnmmommommnonmmonqnmmo") },
	});

	const Bytes fixture = BuildLevelFixture(tokens, 12, payload, 1);
	const SaveRestoreBlockHeader header =
		ParseSaveRestoreBlockHeader(fixture.data(), fixture.size());
	const SaveRestoreTokenTable table =
		ParseSaveRestoreTokenTable(fixture.data(), fixture.size(), header);

	if (header.status != SaveRestoreParseStatus::Ok ||
		header.kind != SaveRestoreBlockKind::Level ||
		header.tableCount != 1 ||
		header.payloadOffset != 24 + tokens.size() ||
		table.status != SaveRestoreParseStatus::Ok ||
		table.tokens[TSaveHeader] != "Save Header")
	{
		return false;
	}

	std::size_t offset = 0;
	SaveRestoreFieldSection section = {};
	const std::uint8_t *payloadData = fixture.data() + header.payloadOffset;
	if (ParseSaveRestoreFieldSection(
			payloadData,
			header.dataSize,
			table.tokens,
			&offset,
			&section) != SaveRestoreParseStatus::Ok ||
		section.name != "ETABLE" ||
		section.fields.size() != 2 ||
		section.fields[1].name != "classname")
	{
		return false;
	}

	if (ParseSaveRestoreFieldSection(
			payloadData,
			header.dataSize,
			table.tokens,
			&offset,
			&section) != SaveRestoreParseStatus::Ok ||
		section.name != "Save Header" ||
		section.fields.size() != 4 ||
		section.fields[2].name != "lightStyleCount")
	{
		return false;
	}

	return ParseSaveRestoreFieldSection(
			payloadData,
			header.dataSize,
			table.tokens,
			&offset,
			&section) == SaveRestoreParseStatus::Ok &&
		section.name == "LIGHTSTYLE" &&
		section.fields.size() == 2 &&
		section.fields[1].name == "style" &&
		offset == static_cast<std::size_t>(header.dataSize);
}

static bool TestTokenTableRebaseCanDifferFromDiskTokenSize()
{
	Bytes tokens = TokenTable({ "GameHeader", "mapName" });
	tokens.push_back('x');
	tokens.push_back('x');

	Bytes payload;
	AppendSection(payload, 0, { { 1, TextData("c1a0") } });

	const Bytes fixture = BuildGameFixture(
		kSaveRestoreGameVersion,
		2,
		tokens,
		payload);
	const SaveRestoreBlockHeader header =
		ParseSaveRestoreBlockHeader(fixture.data(), fixture.size());
	const SaveRestoreTokenTable table =
		ParseSaveRestoreTokenTable(fixture.data(), fixture.size(), header);

	return header.status == SaveRestoreParseStatus::Ok &&
		header.kind == SaveRestoreBlockKind::Game &&
		header.payloadOffset == 20 + tokens.size() &&
		table.status == SaveRestoreParseStatus::Ok &&
		table.consumedBytes == tokens.size() - 2 &&
		table.rebasedPayloadOffset == 20 + tokens.size() - 2;
}

static bool TestClientSaveHeaderVersion()
{
	const Bytes tokens = TokenTable({ "ClientHeader" });
	Bytes payload;
	AppendSection(payload, 0, {});

	const Bytes fixture = BuildGameFixture(
		kSaveRestoreClientVersion,
		1,
		tokens,
		payload);
	const SaveRestoreBlockHeader header =
		ParseSaveRestoreBlockHeader(fixture.data(), fixture.size());

	return header.status == SaveRestoreParseStatus::Ok &&
		header.kind == SaveRestoreBlockKind::Client &&
		header.tableCount == 0;
}

static bool TestBundledFileEntry()
{
	Bytes data(kSaveRestoreBundledNameBytes, 0);
	const char *name = "c1a0.HL1";
	std::memcpy(data.data(), name, std::strlen(name));
	AppendI32(data, 3);
	data.push_back('a');
	data.push_back('b');
	data.push_back('c');

	const SaveRestoreBundledFile file =
		ParseSaveRestoreBundledFile(data.data(), data.size(), 0);

	return file.status == SaveRestoreParseStatus::Ok &&
		file.name == "c1a0.HL1" &&
		file.size == 3 &&
		file.dataOffset == kSaveRestoreBundledNameBytes + 4 &&
		file.nextOffset == data.size();
}

static bool TestMalformedBlocks()
{
	Bytes invalidMagic;
	AppendU32(invalidMagic, 0x12345678u);
	AppendI32(invalidMagic, kSaveRestoreGameVersion);

	Bytes badVersion;
	AppendU32(badVersion, kSaveRestoreGameMagic);
	AppendI32(badVersion, 0x0065);
	AppendI32(badVersion, 0);
	AppendI32(badVersion, 0);
	AppendI32(badVersion, 0);

	Bytes invalidCount;
	AppendU32(invalidCount, kSaveRestoreGameMagic);
	AppendI32(invalidCount, kSaveRestoreGameVersion);
	AppendI32(invalidCount, 0);
	AppendI32(invalidCount, -1);
	AppendI32(invalidCount, 0);

	Bytes missingTerminator;
	AppendU32(missingTerminator, kSaveRestoreGameMagic);
	AppendI32(missingTerminator, kSaveRestoreGameVersion);
	AppendI32(missingTerminator, 0);
	AppendI32(missingTerminator, 1);
	AppendI32(missingTerminator, 3);
	missingTerminator.push_back('b');
	missingTerminator.push_back('a');
	missingTerminator.push_back('d');

	const SaveRestoreBlockHeader missingHeader =
		ParseSaveRestoreBlockHeader(
			missingTerminator.data(),
			missingTerminator.size());
	const SaveRestoreTokenTable missingTokens =
		ParseSaveRestoreTokenTable(
			missingTerminator.data(),
			missingTerminator.size(),
			missingHeader);

	return ParseSaveRestoreBlockHeader(
			invalidMagic.data(),
			invalidMagic.size()).status == SaveRestoreParseStatus::InvalidMagic &&
		ParseSaveRestoreBlockHeader(
			badVersion.data(),
			badVersion.size()).status == SaveRestoreParseStatus::UnsupportedVersion &&
		ParseSaveRestoreBlockHeader(
			invalidCount.data(),
			invalidCount.size()).status == SaveRestoreParseStatus::InvalidCount &&
		missingTokens.status == SaveRestoreParseStatus::MissingTerminator;
}

static bool TestMalformedFieldSectionsAndBundle()
{
	const std::vector<std::string> tokens = { "GameHeader" };
	Bytes payload;
	AppendU16(payload, 1);
	AppendU16(payload, 3);
	payload.push_back(0);

	std::size_t offset = 0;
	SaveRestoreFieldSection section = {};

	Bytes truncatedBundle(kSaveRestoreBundledNameBytes, 0);
	AppendI32(truncatedBundle, 4);
	truncatedBundle.push_back('x');

	return ParseSaveRestoreFieldSection(
			payload.data(),
			payload.size(),
			tokens,
			&offset,
			&section) == SaveRestoreParseStatus::InvalidTokenIndex &&
		ParseSaveRestoreBundledFile(
			truncatedBundle.data(),
			truncatedBundle.size(),
			0).status == SaveRestoreParseStatus::Truncated;
}

static bool TestEntityPatchReportsInvalidIndexes()
{
	Bytes patch;
	AppendI32(patch, 4);
	AppendI32(patch, 0);
	AppendI32(patch, 3);
	AppendI32(patch, -1);
	AppendI32(patch, 2);

	const SaveRestoreEntityPatch parsed =
		ParseSaveRestoreEntityPatch(patch.data(), patch.size(), 3);

	Bytes truncated;
	AppendI32(truncated, 2);
	AppendI32(truncated, 0);

	Bytes negativeCount;
	AppendI32(negativeCount, -1);

	return parsed.status == SaveRestoreParseStatus::InvalidEntityPatchIndex &&
		parsed.patchCount == 4 &&
		parsed.consumedBytes == patch.size() &&
		parsed.removedEntityIndexes.size() == 2 &&
		parsed.removedEntityIndexes[0] == 0 &&
		parsed.removedEntityIndexes[1] == 2 &&
		parsed.invalidEntityIndexes.size() == 2 &&
		parsed.invalidEntityIndexes[0] == 3 &&
		parsed.invalidEntityIndexes[1] == -1 &&
		ParseSaveRestoreEntityPatch(
			truncated.data(),
			truncated.size(),
			3).status == SaveRestoreParseStatus::Truncated &&
		ParseSaveRestoreEntityPatch(
			negativeCount.data(),
			negativeCount.size(),
			3).status == SaveRestoreParseStatus::InvalidCount;
}

static bool TestPackedViewEntityShort()
{
	const std::uint8_t positive[] = { 0x34, 0x12 };
	const std::uint8_t negative[] = { 0xFF, 0xFF };
	const std::uint8_t truncated[] = { 0x01 };

	static_assert(
		sizeof(short) == kSaveRestorePackedShortBytes,
		"SAVE_CLIENT.viewentity is serialized as FIELD_CHARACTER[sizeof(short)]");

	const SaveRestorePackedShort parsedPositive =
		ParseSaveRestorePackedShort(positive, sizeof(positive));
	const SaveRestorePackedShort parsedNegative =
		ParseSaveRestorePackedShort(negative, sizeof(negative));

	return parsedPositive.status == SaveRestoreParseStatus::Ok &&
		parsedPositive.value == 0x1234 &&
		parsedNegative.status == SaveRestoreParseStatus::Ok &&
		parsedNegative.value == static_cast<std::int16_t>(-1) &&
		ParseSaveRestorePackedShort(
			truncated,
			sizeof(truncated)).status == SaveRestoreParseStatus::Truncated;
}

}

int main()
{
	if (!TestLevelHeaderTokenTableAndSections() ||
		!TestTokenTableRebaseCanDifferFromDiskTokenSize() ||
		!TestClientSaveHeaderVersion() ||
		!TestBundledFileEntry() ||
		!TestMalformedBlocks() ||
		!TestMalformedFieldSectionsAndBundle() ||
		!TestEntityPatchReportsInvalidIndexes() ||
		!TestPackedViewEntityShort())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
