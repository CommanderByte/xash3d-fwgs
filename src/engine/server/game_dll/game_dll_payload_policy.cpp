#include "engine/server/game_dll/game_dll_payload_policy.hpp"

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

bool HasFlag(int flags, int flag)
{
	return (flags & flag) != 0;
}

int ClampInt(int value, int minValue, int maxValue)
{
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;
	return value;
}

int EncodeParticleDirection(float value)
{
	return ClampInt(static_cast<int>(value * 16.0f), -128, 127);
}

}

GameDllSoundRoute BuildGameDllStartSoundRoute(
	int flags,
	int channel,
	int maxClients,
	bool quakeCompatible)
{
	GameDllSoundRoute route = {};

	if (HasFlag(flags, kGameDllPayloadSoundSpawningFlag))
		route.destination = kGameDllPayloadDestinationInit;
	else if (channel == kGameDllPayloadChannelStatic)
		route.destination = kGameDllPayloadDestinationAll;
	else if (quakeCompatible)
		route.destination = kGameDllPayloadDestinationAll;
	else
		route.destination = maxClients <= 1 ?
			kGameDllPayloadDestinationAll :
			kGameDllPayloadDestinationPasReliable;

	if (HasFlag(flags, kGameDllPayloadSoundStopFlag))
		route.destination = kGameDllPayloadDestinationAll;

	route.filterClient = HasFlag(flags, kGameDllPayloadSoundFilterClientFlag);
	return route;
}

GameDllAmbientSoundPlan BuildGameDllAmbientSoundPlan(
	bool serverLoading,
	int flags)
{
	GameDllAmbientSoundPlan plan = {};
	plan.flags = flags;

	if (serverLoading)
		plan.flags |= kGameDllPayloadSoundSpawningFlag;

	plan.destination = HasFlag(plan.flags, kGameDllPayloadSoundSpawningFlag) ?
		kGameDllPayloadDestinationInit :
		kGameDllPayloadDestinationAll;

	if (HasFlag(plan.flags, kGameDllPayloadSoundStopFlag))
		plan.destination = kGameDllPayloadDestinationAll;

	return plan;
}

GameDllParticlePlan BuildGameDllParticlePlan(
	int bytesLeft,
	float directionX,
	float directionY,
	float directionZ,
	float count,
	float color)
{
	GameDllParticlePlan plan = {};

	if (bytesLeft < kGameDllParticleMinimumBytesLeft)
		return plan;

	plan.shouldWrite = true;
	plan.direction[0] = EncodeParticleDirection(directionX);
	plan.direction[1] = EncodeParticleDirection(directionY);
	plan.direction[2] = EncodeParticleDirection(directionZ);
	plan.count = static_cast<int>(count);
	plan.color = static_cast<int>(color);
	return plan;
}

bool GameDllParticleHasWritableBuffer(int bytesLeft)
{
	return bytesLeft >= kGameDllParticleMinimumBytesLeft;
}

GameDllLightStylePlan BuildGameDllLightStylePlan(
	int style,
	int maxLightStyles,
	bool loadGame)
{
	GameDllLightStylePlan plan = {};

	if (style < 0)
		style = 0;

	plan.style = style;

	if (style >= maxLightStyles)
	{
		plan.action = GameDllLightStyleAction::FatalStyleOverflow;
		return plan;
	}

	if (loadGame)
	{
		plan.action = GameDllLightStyleAction::SkipLoadGame;
		return plan;
	}

	plan.action = GameDllLightStyleAction::SetStyle;
	return plan;
}

GameDllStaticDecalPlan BuildGameDllStaticDecalPlan(int permanentFlag)
{
	GameDllStaticDecalPlan plan = {};
	plan.flags = permanentFlag;
	plan.scale = kGameDllStaticDecalScale;
	return plan;
}

GameDllMakeStaticAction BuildGameDllMakeStaticAction(bool validEntity)
{
	return validEntity ?
		GameDllMakeStaticAction::CaptureBaseline :
		GameDllMakeStaticAction::IgnoreInvalidEntity;
}

const char *GameDllLightStyleActionName(GameDllLightStyleAction action)
{
	switch (action)
	{
	case GameDllLightStyleAction::SetStyle:
		return "set-style";
	case GameDllLightStyleAction::SkipLoadGame:
		return "skip-load-game";
	case GameDllLightStyleAction::FatalStyleOverflow:
		return "fatal-style-overflow";
	}

	return "unknown";
}

const char *GameDllMakeStaticActionName(GameDllMakeStaticAction action)
{
	switch (action)
	{
	case GameDllMakeStaticAction::IgnoreInvalidEntity:
		return "ignore-invalid-entity";
	case GameDllMakeStaticAction::CaptureBaseline:
		return "capture-baseline";
	}

	return "unknown";
}

}
}
}
