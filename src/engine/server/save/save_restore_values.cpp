#include "engine/server/save/save_restore_values.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

const char *NonNullText(const char *text)
{
	return text ? text : "";
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

}
}
}
