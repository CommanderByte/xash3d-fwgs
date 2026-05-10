#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll_resource_policy.hpp"

using namespace xash::engine::server;

namespace
{

constexpr std::size_t kLegacyCapacity = 16;

static bool TestEmptyNamesAreRejected()
{
	return BuildGameDllResourceNameDecision(
			nullptr,
			GameDllResourceNameMode::ModelPrecache,
			kLegacyCapacity).action == GameDllResourceNameAction::RejectEmpty &&
		BuildGameDllResourceNameDecision(
			"",
			GameDllResourceNameMode::SoundPrecache,
			kLegacyCapacity).action == GameDllResourceNameAction::RejectEmpty &&
		BuildGameDllResourceNameDecision(
			"",
			GameDllResourceNameMode::GenericPrecache,
			kLegacyCapacity).action == GameDllResourceNameAction::RejectEmpty;
}

static bool TestModelOptionalPrefix()
{
	const GameDllResourceNameDecision optional =
		BuildGameDllResourceNameDecision(
			"!models\\barney.mdl",
			GameDllResourceNameMode::ModelPrecache,
			0);
	const GameDllResourceNameDecision lookup =
		BuildGameDllResourceNameDecision(
			"!models\\barney.mdl",
			GameDllResourceNameMode::ModelLookup,
			0);

	return optional.action == GameDllResourceNameAction::UseName &&
		optional.optional &&
		optional.normalizedName == "models/barney.mdl" &&
		lookup.action == GameDllResourceNameAction::UseName &&
		!lookup.optional &&
		lookup.normalizedName == "!models/barney.mdl";
}

static bool TestSoundSentenceNamesAreRejected()
{
	const GameDllResourceNameDecision decision =
		BuildGameDllResourceNameDecision(
			"!HEV_AA0",
			GameDllResourceNameMode::SoundPrecache,
			0);

	return decision.action == GameDllResourceNameAction::RejectSentenceName &&
		!decision.optional &&
		decision.normalizedName.empty();
}

static bool TestSlashNormalizationAndLeadingSlashRules()
{
	const GameDllResourceNameDecision model =
		BuildGameDllResourceNameDecision(
			"\\models\\\\player\\hgrunt.mdl",
			GameDllResourceNameMode::ModelLookup,
			0);
	const GameDllResourceNameDecision sound =
		BuildGameDllResourceNameDecision(
			"/weapons//pl_gun3.wav",
			GameDllResourceNameMode::SoundPrecache,
			0);
	const GameDllResourceNameDecision generic =
		BuildGameDllResourceNameDecision(
			"/sprites\\\\hud640.spr",
			GameDllResourceNameMode::GenericPrecache,
			0);
	const GameDllResourceNameDecision event =
		BuildGameDllResourceNameDecision(
			"/events\\\\gauss.sc",
			GameDllResourceNameMode::EventPrecache,
			0);
	const GameDllResourceNameDecision decal =
		BuildGameDllResourceNameDecision(
			"{broken\\decal",
			GameDllResourceNameMode::DecalLookup,
			0);

	return model.normalizedName == "models/player/hgrunt.mdl" &&
		sound.normalizedName == "weapons/pl_gun3.wav" &&
		generic.normalizedName == "/sprites/hud640.spr" &&
		event.normalizedName == "/events/gauss.sc" &&
		decal.normalizedName == "{broken\\decal";
}

static bool TestLegacyCapacityIsAppliedBeforeSlashFixup()
{
	const GameDllResourceNameDecision decision =
		BuildGameDllResourceNameDecision(
			"models\\\\abcdefghijklmnop.mdl",
			GameDllResourceNameMode::ModelLookup,
			kLegacyCapacity);

	return decision.normalizedName == "models/abcdefg";
}

static bool TestCaseInsensitiveDuplicateLookup()
{
	return GameDllResourceNamesEqual("MODELS/V_AK47.MDL", "models/v_ak47.mdl") &&
		GameDllResourceNamesEqual("sound/WEAPONS/a.wav", "SOUND/weapons/A.WAV") &&
		!GameDllResourceNamesEqual("models/a.mdl", "models/b.mdl") &&
		!GameDllResourceNamesEqual(nullptr, "models/a.mdl") &&
		GameDllResourceNamesEqual(nullptr, nullptr);
}

static bool TestModelPrecacheLoadActions()
{
	return BuildGameDllModelPrecacheLoadAction(false, 0) ==
			GameDllModelPrecacheLoadAction::ReturnZero &&
		BuildGameDllModelPrecacheLoadAction(true, 0) ==
			GameDllModelPrecacheLoadAction::ReturnZero &&
		BuildGameDllModelPrecacheLoadAction(true, 3) ==
			GameDllModelPrecacheLoadAction::LoadOptional &&
		BuildGameDllModelPrecacheLoadAction(false, 3) ==
			GameDllModelPrecacheLoadAction::LoadFatalIfMissing;
}

static bool TestSlotBoundsPolicy()
{
	return BuildGameDllResourceSlotAction(0, 4) ==
			GameDllResourceSlotAction::UseSlot &&
		BuildGameDllResourceSlotAction(3, 4) ==
			GameDllResourceSlotAction::UseSlot &&
		BuildGameDllResourceSlotAction(4, 4) ==
			GameDllResourceSlotAction::FatalLimitExceeded &&
		BuildGameDllResourceSlotAction(5, 4) ==
			GameDllResourceSlotAction::FatalLimitExceeded;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllResourceNameActionName(
				GameDllResourceNameAction::RejectSentenceName),
			"reject-sentence-name") == 0 &&
		std::strcmp(
			GameDllResourceSlotActionName(
				GameDllResourceSlotAction::FatalLimitExceeded),
			"fatal-limit-exceeded") == 0 &&
		std::strcmp(
			GameDllModelPrecacheLoadActionName(
				GameDllModelPrecacheLoadAction::LoadFatalIfMissing),
			"load-fatal-if-missing") == 0;
}

}

int main()
{
	if (!TestEmptyNamesAreRejected() ||
		!TestModelOptionalPrefix() ||
		!TestSoundSentenceNamesAreRejected() ||
		!TestSlashNormalizationAndLeadingSlashRules() ||
		!TestLegacyCapacityIsAppliedBeforeSlashFixup() ||
		!TestCaseInsensitiveDuplicateLookup() ||
		!TestModelPrecacheLoadActions() ||
		!TestSlotBoundsPolicy() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
