#include "game_dll_payload_policy_adapter.h"

#include "const.h"
#include "protocol.h"
#include "engine/server/game_dll/game_dll_payload_policy.hpp"

static_assert(MSG_ALL == xash::engine::server::kGameDllPayloadDestinationAll,
	"MSG_ALL value changed");
static_assert(MSG_INIT == xash::engine::server::kGameDllPayloadDestinationInit,
	"MSG_INIT value changed");
static_assert(MSG_PAS_R == xash::engine::server::kGameDllPayloadDestinationPasReliable,
	"MSG_PAS_R value changed");
static_assert(CHAN_STATIC == xash::engine::server::kGameDllPayloadChannelStatic,
	"CHAN_STATIC value changed");
static_assert(SND_STOP == xash::engine::server::kGameDllPayloadSoundStopFlag,
	"SND_STOP value changed");
static_assert(SND_SPAWNING == xash::engine::server::kGameDllPayloadSoundSpawningFlag,
	"SND_SPAWNING value changed");
static_assert(SND_FILTER_CLIENT == xash::engine::server::kGameDllPayloadSoundFilterClientFlag,
	"SND_FILTER_CLIENT value changed");

namespace
{

int ToLegacyLightStyleAction(
	xash::engine::server::GameDllLightStyleAction action)
{
	using xash::engine::server::GameDllLightStyleAction;

	switch (action)
	{
	case GameDllLightStyleAction::SetStyle:
		return SV_GAMEDLL_LIGHTSTYLE_SET_STYLE;
	case GameDllLightStyleAction::SkipLoadGame:
		return SV_GAMEDLL_LIGHTSTYLE_SKIP_LOADGAME;
	case GameDllLightStyleAction::FatalStyleOverflow:
	default:
		return SV_GAMEDLL_LIGHTSTYLE_FATAL_STYLE_OVERFLOW;
	}
}

int ToLegacyMakeStaticAction(
	xash::engine::server::GameDllMakeStaticAction action)
{
	using xash::engine::server::GameDllMakeStaticAction;

	switch (action)
	{
	case GameDllMakeStaticAction::CaptureBaseline:
		return SV_GAMEDLL_MAKE_STATIC_CAPTURE_BASELINE;
	case GameDllMakeStaticAction::IgnoreInvalidEntity:
	default:
		return SV_GAMEDLL_MAKE_STATIC_IGNORE_INVALID_ENTITY;
	}
}

}

extern "C" sv_gamedll_sound_route_t SV_GameDllPayload_BuildStartSoundRoute(
	int flags,
	int channel,
	int max_clients,
	int quake_compatible)
{
	const xash::engine::server::GameDllSoundRoute modern =
		xash::engine::server::BuildGameDllStartSoundRoute(
			flags,
			channel,
			max_clients,
			quake_compatible != 0);

	sv_gamedll_sound_route_t legacy = {};
	legacy.destination = modern.destination;
	legacy.filter_client = modern.filterClient ? 1 : 0;
	return legacy;
}

extern "C" sv_gamedll_ambient_sound_plan_t
SV_GameDllPayload_BuildAmbientSoundPlan(int server_loading, int flags)
{
	const xash::engine::server::GameDllAmbientSoundPlan modern =
		xash::engine::server::BuildGameDllAmbientSoundPlan(
			server_loading != 0,
			flags);

	sv_gamedll_ambient_sound_plan_t legacy = {};
	legacy.flags = modern.flags;
	legacy.destination = modern.destination;
	return legacy;
}

extern "C" sv_gamedll_particle_plan_t SV_GameDllPayload_BuildParticlePlan(
	int bytes_left,
	float direction_x,
	float direction_y,
	float direction_z,
	float count,
	float color)
{
	const xash::engine::server::GameDllParticlePlan modern =
		xash::engine::server::BuildGameDllParticlePlan(
			bytes_left,
			direction_x,
			direction_y,
			direction_z,
			count,
			color);

	sv_gamedll_particle_plan_t legacy = {};
	legacy.should_write = modern.shouldWrite ? 1 : 0;
	legacy.direction_x = modern.direction[0];
	legacy.direction_y = modern.direction[1];
	legacy.direction_z = modern.direction[2];
	legacy.count = modern.count;
	legacy.color = modern.color;
	return legacy;
}

extern "C" int SV_GameDllPayload_ParticleHasWritableBuffer(int bytes_left)
{
	return xash::engine::server::GameDllParticleHasWritableBuffer(bytes_left) ? 1 : 0;
}

extern "C" sv_gamedll_lightstyle_plan_t SV_GameDllPayload_BuildLightStylePlan(
	int style,
	int max_lightstyles,
	int loadgame)
{
	const xash::engine::server::GameDllLightStylePlan modern =
		xash::engine::server::BuildGameDllLightStylePlan(
			style,
			max_lightstyles,
			loadgame != 0);

	sv_gamedll_lightstyle_plan_t legacy = {};
	legacy.action = ToLegacyLightStyleAction(modern.action);
	legacy.style = modern.style;
	return legacy;
}

extern "C" sv_gamedll_static_decal_plan_t
SV_GameDllPayload_BuildStaticDecalPlan(int permanent_flag)
{
	const xash::engine::server::GameDllStaticDecalPlan modern =
		xash::engine::server::BuildGameDllStaticDecalPlan(permanent_flag);

	sv_gamedll_static_decal_plan_t legacy = {};
	legacy.flags = modern.flags;
	legacy.scale = modern.scale;
	return legacy;
}

extern "C" int SV_GameDllPayload_BuildMakeStaticAction(int valid_entity)
{
	return ToLegacyMakeStaticAction(
		xash::engine::server::BuildGameDllMakeStaticAction(valid_entity != 0));
}
