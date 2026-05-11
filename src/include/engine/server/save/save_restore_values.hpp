#ifndef XASH_ENGINE_SERVER_SAVE_RESTORE_VALUES_HPP
#define XASH_ENGINE_SERVER_SAVE_RESTORE_VALUES_HPP

#include <cstdint>

#include "engine/server/save/save_restore_format.hpp"

namespace xash
{
namespace engine
{
namespace server
{

constexpr int kSaveRestoreOldUnsupportedVersion = 0x0065;

enum class SaveAdmissionDecision
{
	Allow = 0,
	NotPlayingLocalGame = 1,
	BackgroundOrCredits = 2,
	PhysicsVeto = 3,
	InactiveClient = 4,
	Intermission = 5,
	Multiplayer = 6,
	MissingClient = 7,
	MissingPlayer = 8,
	DeadPlayer = 9,
};

struct SaveAdmissionSnapshot
{
	bool serverInitialized;
	bool serverActive;
	bool backgroundMap;
	bool creditsActive;
	bool physicsCallbackAvailable;
	bool physicsAllowsSave;
	bool clientActive;
	bool intermission;
	int maxClients;
	bool spawnedClientAvailable;
	bool playerEdictAvailable;
	bool playerDeadFlag;
	float playerHealth;
};

struct SaveAdmissionPlan
{
	SaveAdmissionDecision decision;
	bool allowed;
	const char *legacyConsoleMessage;
};

enum class SaveGameCommentHeaderDecision
{
	ReadFields = 0,
	ClearMissingFile = 1,
	CorruptedHeader = 2,
	OldUnsupportedVersion = 3,
	OldVersion = 4,
	InvalidVersion = 5,
};

struct SaveGameCommentHeaderPlan
{
	SaveGameCommentHeaderDecision decision;
	bool shouldReadFields;
	const char *legacyCommentText;
};

enum class SaveCommentSource
{
	GameDllOverride = 0,
	TitleAlias = 1,
	WorldMessage = 2,
	MapName = 3,
};

struct SaveCommentFallbackSnapshot
{
	bool gameDllCommentAvailable;
	const char *gameDllComment;
	const char *titleAlias;
	bool worldMessageAvailable;
	const char *worldMessage;
	const char *mapName;
	double elapsedSeconds;
};

struct SaveCommentFallbackPlan
{
	SaveCommentSource source;
	const char *sourceText;
	int elapsedMinutes;
	int elapsedSecondRemainder;
};

SaveAdmissionPlan BuildSaveAdmissionPlan(
	const SaveAdmissionSnapshot &snapshot);
const char *SaveAdmissionDecisionName(SaveAdmissionDecision decision);

SaveGameCommentHeaderPlan BuildSaveGameCommentHeaderPlan(
	bool fileOpened,
	std::uint32_t magic,
	int version);
const char *SaveGameCommentHeaderDecisionName(
	SaveGameCommentHeaderDecision decision);

SaveCommentFallbackPlan BuildSaveCommentFallbackPlan(
	const SaveCommentFallbackSnapshot &snapshot);
const char *SaveCommentSourceName(SaveCommentSource source);

}
}
}

#endif
