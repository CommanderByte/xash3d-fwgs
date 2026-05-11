#include <cstdlib>
#include <cstring>
#include <vector>

#include "engine/server/save_restore_runtime.hpp"
#include "engine/server/save_restore_values.hpp"

using namespace xash::engine::server;

namespace
{

using Bytes = std::vector<std::uint8_t>;

static bool TextEquals(const char *left, const char *right)
{
	if (!left || !right)
		return left == right;

	return std::strcmp(left, right) == 0;
}

static void AppendI32(Bytes &out, int value)
{
	const std::uint32_t bits = static_cast<std::uint32_t>(value);
	out.push_back(static_cast<std::uint8_t>(bits & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xFF));
	out.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xFF));
}

static void AppendBundledFile(Bytes &out, const char *name, const Bytes &data)
{
	const std::size_t start = out.size();
	out.resize(out.size() + kSaveRestoreBundledNameBytes, 0);
	std::memcpy(
		out.data() + start,
		name,
		std::strlen(name));
	AppendI32(out, static_cast<int>(data.size()));
	for (std::size_t i = 0; i < data.size(); ++i)
		out.push_back(data[i]);
}

static SaveAdmissionSnapshot PassingAdmission()
{
	SaveAdmissionSnapshot snapshot = {};
	snapshot.serverInitialized = true;
	snapshot.serverActive = true;
	snapshot.clientActive = true;
	snapshot.maxClients = 1;
	snapshot.spawnedClientAvailable = true;
	snapshot.playerEdictAvailable = true;
	snapshot.playerHealth = 100.0f;
	return snapshot;
}

static bool TestRuntimeFixtureComposesAdmissionCommentPatchAndManifest()
{
	const SaveAdmissionPlan admission =
		BuildSaveAdmissionPlan(PassingAdmission());
	const SaveGameCommentHeaderPlan commentHeader =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreGameMagic,
			kSaveRestoreGameVersion);

	SaveEntityTableRowSnapshot rows[4] = {};
	rows[1].removed = true;
	rows[3].removed = true;
	const SaveEntityPatchWritePlan writePlan =
		BuildSaveEntityPatchWritePlan(rows, 4);

	Bytes patch;
	AppendI32(patch, 2);
	AppendI32(patch, 1);
	AppendI32(patch, 3);
	const SaveEntityPatchApplyPlan applyPlan =
		BuildSaveEntityPatchApplyPlan(patch.data(), patch.size(), 4);

	Bytes archive;
	AppendBundledFile(archive, "c1a0.HL1", { 'h', 'l', '1' });
	AppendBundledFile(archive, "c1a0.HL2", { 'h', '2' });
	const SaveArchiveManifest manifest =
		ParseSaveArchiveManifest(archive.data(), archive.size(), 0, 2);

	return admission.allowed &&
		commentHeader.shouldReadFields &&
		writePlan.removedEntityIndexes.size() == 2 &&
		writePlan.removedEntityIndexes[0] == 1 &&
		writePlan.removedEntityIndexes[1] == 3 &&
		applyPlan.status == SaveRestoreParseStatus::Ok &&
		applyPlan.markRemovedEntityIndexes == writePlan.removedEntityIndexes &&
		manifest.status == SaveRestoreParseStatus::Ok &&
		manifest.entries.size() == 2 &&
		manifest.entries[0].name == "c1a0.HL1" &&
		manifest.entries[0].size == 3 &&
		manifest.entries[1].name == "c1a0.HL2" &&
		manifest.entries[1].size == 2 &&
		manifest.consumedBytes == archive.size();
}

static bool TestRuntimeFixtureCapturesInvalidInputsWithoutSideEffects()
{
	SaveAdmissionSnapshot snapshot = PassingAdmission();
	snapshot.serverActive = false;
	const SaveAdmissionPlan admission =
		BuildSaveAdmissionPlan(snapshot);
	const SaveGameCommentHeaderPlan corruptedHeader =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreLevelMagic,
			kSaveRestoreGameVersion);

	Bytes badPatch;
	AppendI32(badPatch, 3);
	AppendI32(badPatch, 0);
	AppendI32(badPatch, 7);
	AppendI32(badPatch, -1);
	const SaveEntityPatchApplyPlan patch =
		BuildSaveEntityPatchApplyPlan(badPatch.data(), badPatch.size(), 2);

	Bytes truncatedArchive(kSaveRestoreBundledNameBytes, 0);
	AppendI32(truncatedArchive, 8);
	truncatedArchive.push_back('x');
	const SaveArchiveManifest manifest =
		ParseSaveArchiveManifest(
			truncatedArchive.data(),
			truncatedArchive.size(),
			0,
			1);
	const SaveArchiveManifest negativeManifest =
		ParseSaveArchiveManifest(nullptr, 0, 0, -1);

	return !admission.allowed &&
		admission.decision == SaveAdmissionDecision::NotPlayingLocalGame &&
		corruptedHeader.decision ==
			SaveGameCommentHeaderDecision::CorruptedHeader &&
		TextEquals(corruptedHeader.legacyCommentText, "<corrupted>") &&
		patch.status == SaveRestoreParseStatus::InvalidEntityPatchIndex &&
		patch.markRemovedEntityIndexes.size() == 1 &&
		patch.markRemovedEntityIndexes[0] == 0 &&
		patch.invalidEntityIndexes.size() == 2 &&
		patch.invalidEntityIndexes[0] == 7 &&
		patch.invalidEntityIndexes[1] == -1 &&
		manifest.status == SaveRestoreParseStatus::Truncated &&
		manifest.consumedBytes == 0 &&
		negativeManifest.status == SaveRestoreParseStatus::InvalidCount;
}

static bool TestCommentFallbackRemainsPlainValueData()
{
	SaveCommentFallbackSnapshot snapshot = {};
	snapshot.titleAlias = "#C1A0TITLE";
	snapshot.worldMessageAvailable = true;
	snapshot.worldMessage = "world message";
	snapshot.mapName = "c1a0";
	snapshot.elapsedSeconds = 599.8;

	const SaveCommentFallbackPlan title =
		BuildSaveCommentFallbackPlan(snapshot);
	snapshot.titleAlias = "";
	const SaveCommentFallbackPlan world =
		BuildSaveCommentFallbackPlan(snapshot);
	snapshot.worldMessageAvailable = false;
	const SaveCommentFallbackPlan map =
		BuildSaveCommentFallbackPlan(snapshot);

	return title.source == SaveCommentSource::TitleAlias &&
		TextEquals(title.sourceText, "#C1A0TITLE") &&
		title.elapsedMinutes == 9 &&
		title.elapsedSecondRemainder == 59 &&
		world.source == SaveCommentSource::WorldMessage &&
		TextEquals(world.sourceText, "world message") &&
		map.source == SaveCommentSource::MapName &&
		TextEquals(map.sourceText, "c1a0");
}

}

int main()
{
	if (!TestRuntimeFixtureComposesAdmissionCommentPatchAndManifest() ||
		!TestRuntimeFixtureCapturesInvalidInputsWithoutSideEffects() ||
		!TestCommentFallbackRemainsPlainValueData())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
