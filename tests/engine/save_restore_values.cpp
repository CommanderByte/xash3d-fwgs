#include <cstdlib>
#include <cstring>

#include "engine/server/save_restore_values.hpp"

using namespace xash::engine::server;

namespace
{

static bool TextEquals(const char *left, const char *right)
{
	if (!left || !right)
		return left == right;

	return std::strcmp(left, right) == 0;
}

static SaveAdmissionSnapshot PassingAdmission()
{
	SaveAdmissionSnapshot snapshot = {};
	snapshot.serverInitialized = true;
	snapshot.serverActive = true;
	snapshot.physicsAllowsSave = false;
	snapshot.clientActive = true;
	snapshot.maxClients = 1;
	snapshot.spawnedClientAvailable = true;
	snapshot.playerEdictAvailable = true;
	snapshot.playerHealth = 100.0f;
	return snapshot;
}

static bool TestSaveAdmissionAllowsSingleplayer()
{
	const SaveAdmissionPlan plan =
		BuildSaveAdmissionPlan(PassingAdmission());

	return plan.allowed &&
		plan.decision == SaveAdmissionDecision::Allow &&
		plan.legacyConsoleMessage == nullptr &&
		TextEquals(SaveAdmissionDecisionName(plan.decision), "allow");
}

static bool TestSaveAdmissionPreservesDecisionOrder()
{
	SaveAdmissionSnapshot snapshot = PassingAdmission();
	snapshot.serverInitialized = false;
	snapshot.backgroundMap = true;

	const SaveAdmissionPlan notPlaying =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.backgroundMap = true;
	snapshot.physicsCallbackAvailable = true;
	snapshot.physicsAllowsSave = false;

	const SaveAdmissionPlan silentBackground =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.physicsCallbackAvailable = true;
	snapshot.physicsAllowsSave = false;

	const SaveAdmissionPlan physics =
		BuildSaveAdmissionPlan(snapshot);

	return !notPlaying.allowed &&
		notPlaying.decision == SaveAdmissionDecision::NotPlayingLocalGame &&
		TextEquals(
			notPlaying.legacyConsoleMessage,
			"Not playing a local game.\n") &&
		!silentBackground.allowed &&
		silentBackground.decision ==
			SaveAdmissionDecision::BackgroundOrCredits &&
		silentBackground.legacyConsoleMessage == nullptr &&
		!physics.allowed &&
		physics.decision == SaveAdmissionDecision::PhysicsVeto &&
		TextEquals(
			physics.legacyConsoleMessage,
			"Savegame is not allowed.\n");
}

static bool TestSaveAdmissionClientAndPlayerFailures()
{
	SaveAdmissionSnapshot snapshot = PassingAdmission();
	snapshot.clientActive = false;
	const SaveAdmissionPlan inactive =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.intermission = true;
	const SaveAdmissionPlan intermission =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.maxClients = 2;
	const SaveAdmissionPlan multiplayer =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.spawnedClientAvailable = false;
	const SaveAdmissionPlan missingClient =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.playerEdictAvailable = false;
	const SaveAdmissionPlan missingPlayer =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.playerHealth = 0.0f;
	const SaveAdmissionPlan deadByHealth =
		BuildSaveAdmissionPlan(snapshot);

	snapshot = PassingAdmission();
	snapshot.playerDeadFlag = true;
	const SaveAdmissionPlan deadByFlag =
		BuildSaveAdmissionPlan(snapshot);

	return inactive.decision == SaveAdmissionDecision::InactiveClient &&
		TextEquals(
			inactive.legacyConsoleMessage,
			"Can't save if not active.\n") &&
		intermission.decision == SaveAdmissionDecision::Intermission &&
		TextEquals(
			intermission.legacyConsoleMessage,
			"Can't save during intermission.\n") &&
		multiplayer.decision == SaveAdmissionDecision::Multiplayer &&
		TextEquals(
			multiplayer.legacyConsoleMessage,
			"Can't save multiplayer games.\n") &&
		missingClient.decision == SaveAdmissionDecision::MissingClient &&
		TextEquals(
			missingClient.legacyConsoleMessage,
			"Can't savegame without a client!\n") &&
		missingPlayer.decision == SaveAdmissionDecision::MissingPlayer &&
		TextEquals(
			missingPlayer.legacyConsoleMessage,
			"Can't savegame without a player!\n") &&
		deadByHealth.decision == SaveAdmissionDecision::DeadPlayer &&
		deadByFlag.decision == SaveAdmissionDecision::DeadPlayer &&
		TextEquals(
			deadByHealth.legacyConsoleMessage,
			"Can't savegame with a dead player\n");
}

static bool TestSaveGameCommentHeaderClassification()
{
	const SaveGameCommentHeaderPlan missing =
		BuildSaveGameCommentHeaderPlan(
			false,
			kSaveRestoreGameMagic,
			kSaveRestoreGameVersion);
	const SaveGameCommentHeaderPlan corrupted =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreLevelMagic,
			kSaveRestoreGameVersion);
	const SaveGameCommentHeaderPlan unsupported =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreGameMagic,
			kSaveRestoreOldUnsupportedVersion);
	const SaveGameCommentHeaderPlan old =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreGameMagic,
			kSaveRestoreGameVersion - 1);
	const SaveGameCommentHeaderPlan newer =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreGameMagic,
			kSaveRestoreGameVersion + 1);
	const SaveGameCommentHeaderPlan valid =
		BuildSaveGameCommentHeaderPlan(
			true,
			kSaveRestoreGameMagic,
			kSaveRestoreGameVersion);

	return missing.decision ==
			SaveGameCommentHeaderDecision::ClearMissingFile &&
		!missing.shouldReadFields &&
		TextEquals(missing.legacyCommentText, "") &&
		corrupted.decision ==
			SaveGameCommentHeaderDecision::CorruptedHeader &&
		TextEquals(corrupted.legacyCommentText, "<corrupted>") &&
		unsupported.decision ==
			SaveGameCommentHeaderDecision::OldUnsupportedVersion &&
		TextEquals(
			unsupported.legacyCommentText,
			"<old version Xash3D FWGS unsupported>") &&
		old.decision == SaveGameCommentHeaderDecision::OldVersion &&
		TextEquals(old.legacyCommentText, "<old version>") &&
		newer.decision == SaveGameCommentHeaderDecision::InvalidVersion &&
		TextEquals(newer.legacyCommentText, "<invalid version>") &&
		valid.decision == SaveGameCommentHeaderDecision::ReadFields &&
		valid.shouldReadFields &&
		valid.legacyCommentText == nullptr &&
		TextEquals(
			SaveGameCommentHeaderDecisionName(valid.decision),
			"read_fields");
}

static bool TestSaveCommentFallbackSelection()
{
	SaveCommentFallbackSnapshot snapshot = {};
	snapshot.gameDllCommentAvailable = true;
	snapshot.gameDllComment = "dll comment";
	snapshot.titleAlias = "#TITLE";
	snapshot.worldMessageAvailable = true;
	snapshot.worldMessage = "world message";
	snapshot.mapName = "c1a0";
	snapshot.elapsedSeconds = 125.9;

	const SaveCommentFallbackPlan gameDll =
		BuildSaveCommentFallbackPlan(snapshot);

	snapshot.gameDllCommentAvailable = false;
	const SaveCommentFallbackPlan title =
		BuildSaveCommentFallbackPlan(snapshot);

	snapshot.titleAlias = "";
	const SaveCommentFallbackPlan world =
		BuildSaveCommentFallbackPlan(snapshot);

	snapshot.worldMessageAvailable = false;
	const SaveCommentFallbackPlan map =
		BuildSaveCommentFallbackPlan(snapshot);

	return gameDll.source == SaveCommentSource::GameDllOverride &&
		TextEquals(gameDll.sourceText, "dll comment") &&
		gameDll.elapsedMinutes == 2 &&
		gameDll.elapsedSecondRemainder == 5 &&
		title.source == SaveCommentSource::TitleAlias &&
		TextEquals(title.sourceText, "#TITLE") &&
		world.source == SaveCommentSource::WorldMessage &&
		TextEquals(world.sourceText, "world message") &&
		map.source == SaveCommentSource::MapName &&
		TextEquals(map.sourceText, "c1a0") &&
		TextEquals(SaveCommentSourceName(map.source), "map_name");
}

}

int main()
{
	if (!TestSaveAdmissionAllowsSingleplayer() ||
		!TestSaveAdmissionPreservesDecisionOrder() ||
		!TestSaveAdmissionClientAndPlayerFailures() ||
		!TestSaveGameCommentHeaderClassification() ||
		!TestSaveCommentFallbackSelection())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
