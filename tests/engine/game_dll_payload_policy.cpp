#include <cstdlib>
#include <cstring>

#include "engine/server/game_dll/game_dll_payload_policy.hpp"

using namespace xash::engine::server;

namespace
{

static bool TestStartSoundRouting()
{
	const GameDllSoundRoute spawning = BuildGameDllStartSoundRoute(
		kGameDllPayloadSoundSpawningFlag,
		2,
		8,
		false);
	const GameDllSoundRoute staticChannel = BuildGameDllStartSoundRoute(
		0,
		kGameDllPayloadChannelStatic,
		8,
		false);
	const GameDllSoundRoute quake = BuildGameDllStartSoundRoute(
		0,
		2,
		8,
		true);
	const GameDllSoundRoute singlePlayer = BuildGameDllStartSoundRoute(
		0,
		2,
		1,
		false);
	const GameDllSoundRoute multiplayer = BuildGameDllStartSoundRoute(
		0,
		2,
		8,
		false);

	return spawning.destination == kGameDllPayloadDestinationInit &&
		!spawning.filterClient &&
		staticChannel.destination == kGameDllPayloadDestinationAll &&
		quake.destination == kGameDllPayloadDestinationAll &&
		singlePlayer.destination == kGameDllPayloadDestinationAll &&
		multiplayer.destination == kGameDllPayloadDestinationPasReliable;
}

static bool TestStartSoundStopAndFilterOverride()
{
	const GameDllSoundRoute route = BuildGameDllStartSoundRoute(
		kGameDllPayloadSoundStopFlag |
			kGameDllPayloadSoundFilterClientFlag,
		2,
		8,
		false);

	return route.destination == kGameDllPayloadDestinationAll &&
		route.filterClient;
}

static bool TestAmbientSoundRouting()
{
	const GameDllAmbientSoundPlan loading =
		BuildGameDllAmbientSoundPlan(true, 0);
	const GameDllAmbientSoundPlan explicitSpawning =
		BuildGameDllAmbientSoundPlan(false, kGameDllPayloadSoundSpawningFlag);
	const GameDllAmbientSoundPlan normal =
		BuildGameDllAmbientSoundPlan(false, 0);
	const GameDllAmbientSoundPlan stop =
		BuildGameDllAmbientSoundPlan(
			true,
			kGameDllPayloadSoundStopFlag);

	return (loading.flags & kGameDllPayloadSoundSpawningFlag) != 0 &&
		loading.destination == kGameDllPayloadDestinationInit &&
		explicitSpawning.destination == kGameDllPayloadDestinationInit &&
		normal.destination == kGameDllPayloadDestinationAll &&
		stop.destination == kGameDllPayloadDestinationAll;
}

static bool TestParticlePlan()
{
	const GameDllParticlePlan small =
		BuildGameDllParticlePlan(
			kGameDllParticleMinimumBytesLeft - 1,
			1.0f,
			1.0f,
			1.0f,
			8.0f,
			9.0f);
	const GameDllParticlePlan plan =
		BuildGameDllParticlePlan(
			kGameDllParticleMinimumBytesLeft,
			8.5f,
			-9.0f,
			0.49f,
			12.75f,
			44.25f);

	return !small.shouldWrite &&
		!GameDllParticleHasWritableBuffer(kGameDllParticleMinimumBytesLeft - 1) &&
		GameDllParticleHasWritableBuffer(kGameDllParticleMinimumBytesLeft) &&
		plan.shouldWrite &&
		plan.direction[0] == 127 &&
		plan.direction[1] == -128 &&
		plan.direction[2] == 7 &&
		plan.count == 12 &&
		plan.color == 44;
}

static bool TestLightStylePlan()
{
	const GameDllLightStylePlan negative =
		BuildGameDllLightStylePlan(-7, 256, false);
	const GameDllLightStylePlan loadgame =
		BuildGameDllLightStylePlan(12, 256, true);
	const GameDllLightStylePlan overflow =
		BuildGameDllLightStylePlan(256, 256, false);

	return negative.action == GameDllLightStyleAction::SetStyle &&
		negative.style == 0 &&
		loadgame.action == GameDllLightStyleAction::SkipLoadGame &&
		loadgame.style == 12 &&
		overflow.action == GameDllLightStyleAction::FatalStyleOverflow &&
		overflow.style == 256;
}

static bool TestStaticDecalAndMakeStaticDefaults()
{
	const GameDllStaticDecalPlan decal = BuildGameDllStaticDecalPlan(0x01);

	return decal.flags == 0x01 &&
		decal.scale == kGameDllStaticDecalScale &&
		BuildGameDllMakeStaticAction(false) ==
			GameDllMakeStaticAction::IgnoreInvalidEntity &&
		BuildGameDllMakeStaticAction(true) ==
			GameDllMakeStaticAction::CaptureBaseline;
}

static bool TestDisplayNames()
{
	return std::strcmp(
			GameDllLightStyleActionName(
				GameDllLightStyleAction::FatalStyleOverflow),
			"fatal-style-overflow") == 0 &&
		std::strcmp(
			GameDllMakeStaticActionName(
				GameDllMakeStaticAction::CaptureBaseline),
			"capture-baseline") == 0;
}

}

int main()
{
	if (!TestStartSoundRouting() ||
		!TestStartSoundStopAndFilterOverride() ||
		!TestAmbientSoundRouting() ||
		!TestParticlePlan() ||
		!TestLightStylePlan() ||
		!TestStaticDecalAndMakeStaticDefaults() ||
		!TestDisplayNames())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
