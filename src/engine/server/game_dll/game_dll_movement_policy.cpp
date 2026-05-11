#include "engine/server/game_dll/game_dll_movement_policy.hpp"

#include <cmath>
#include <cstdio>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr float kDegreesToRadians = 0.01745329251994329576923690768489f;
constexpr unsigned int kMovableFlags =
	kGameDllMoveFlagFly | kGameDllMoveFlagSwim | kGameDllMoveFlagOnGround;

bool HasAnyFlag(unsigned int flags, unsigned int mask)
{
	return (flags & mask) != 0;
}

bool CanMove(unsigned int flags)
{
	return HasAnyFlag(flags, kMovableFlags);
}

float LegacyAngleMod(float angle)
{
	return (360.0f / 65536.0f) *
		(static_cast<int>(angle * (65536.0f / 360.0f)) & 65535);
}

GameDllVector3 ZeroVector()
{
	GameDllVector3 value = {};
	return value;
}

std::string FormatMaxspeed(float maxspeed)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%.0f", maxspeed);
	return std::string(buffer);
}

}

GameDllMoveToOriginPlan BuildGameDllMoveToOriginPlan(
	bool goalPresent,
	bool validEntity,
	unsigned int flags,
	int moveType)
{
	GameDllMoveToOriginPlan plan = {};

	if (!goalPresent)
	{
		plan.action = GameDllMoveToOriginAction::SkipMissingGoal;
		return plan;
	}

	if (!validEntity)
	{
		plan.action = GameDllMoveToOriginAction::SkipInvalidEntity;
		return plan;
	}

	if (!CanMove(flags))
	{
		plan.action = GameDllMoveToOriginAction::SkipImmobileEntity;
		return plan;
	}

	if (moveType == kGameDllMoveNormal)
	{
		plan.action = GameDllMoveToOriginAction::StepTowardIdealYaw;
		return plan;
	}

	plan.action = GameDllMoveToOriginAction::FlyDirectionTowardGoal;
	plan.usesVerticalGoal =
		HasAnyFlag(flags, kGameDllMoveFlagFly | kGameDllMoveFlagSwim);
	return plan;
}

GameDllAngularMovePlan BuildGameDllAngularMovePlan(
	bool validEntity,
	float ideal,
	float current,
	float speed)
{
	GameDllAngularMovePlan plan = {};

	if (!validEntity)
	{
		plan.action = GameDllAngularMoveAction::SkipInvalidEntity;
		return plan;
	}

	plan.action = GameDllAngularMoveAction::ApplyAngle;
	current = LegacyAngleMod(current);

	if (current == ideal)
	{
		plan.nextAngle = current;
		return plan;
	}

	float move = ideal - current;

	if (ideal > current)
	{
		if (move >= 180.0f)
			move -= 360.0f;
	}
	else
	{
		if (move <= -180.0f)
			move += 360.0f;
	}

	if (move > 0.0f)
	{
		if (move > speed)
			move = speed;
	}
	else if (move < -speed)
	{
		move = -speed;
	}

	plan.nextAngle = LegacyAngleMod(current + move);
	return plan;
}

GameDllWalkMovePlan BuildGameDllWalkMovePlan(
	bool validEntity,
	unsigned int flags,
	float yaw,
	float distance,
	int mode)
{
	GameDllWalkMovePlan plan = {};
	plan.move = ZeroVector();

	if (!validEntity)
	{
		plan.action = GameDllWalkMoveAction::SkipInvalidEntity;
		return plan;
	}

	if (!CanMove(flags))
	{
		plan.action = GameDllWalkMoveAction::SkipImmobileEntity;
		return plan;
	}

	const float radians = yaw * kDegreesToRadians;
	plan.move.x = std::cos(radians) * distance;
	plan.move.y = std::sin(radians) * distance;
	plan.move.z = 0.0f;

	switch (mode)
	{
	case kGameDllWalkMoveNormal:
		plan.action = GameDllWalkMoveAction::MoveStepRelink;
		break;
	case kGameDllWalkMoveWorldOnly:
		plan.action = GameDllWalkMoveAction::MoveTestWorldOnly;
		break;
	case kGameDllWalkMoveCheckOnly:
		plan.action = GameDllWalkMoveAction::MoveStepCheckOnly;
		break;
	default:
		plan.action = GameDllWalkMoveAction::FatalUnknownMode;
		break;
	}

	return plan;
}

GameDllSetOriginAction BuildGameDllSetOriginAction(bool validEntity)
{
	return validEntity ?
		GameDllSetOriginAction::CopyOriginAndRelink :
		GameDllSetOriginAction::SkipInvalidEntity;
}

GameDllClientMaxspeedPlan BuildGameDllClientMaxspeedPlan(
	bool hasClient,
	float requestedMaxspeed,
	float movevarsMaxspeed)
{
	GameDllClientMaxspeedPlan plan = {};

	if (!hasClient)
	{
		plan.action = GameDllClientMaxspeedAction::SkipMissingClient;
		return plan;
	}

	plan.action = GameDllClientMaxspeedAction::SetMaxspeed;
	plan.maxspeed = requestedMaxspeed;

	if (plan.maxspeed > movevarsMaxspeed)
		plan.maxspeed = movevarsMaxspeed;
	else if (plan.maxspeed < -movevarsMaxspeed)
		plan.maxspeed = -movevarsMaxspeed;

	plan.physInfoValue = FormatMaxspeed(plan.maxspeed);
	return plan;
}

GameDllRunPlayerMovePlan BuildGameDllRunPlayerMovePlan(
	bool hasSpawnedClient,
	bool fakeClient,
	double serverTime,
	double frameTime,
	const GameDllVector3 &viewAngles,
	float forwardMove,
	float sideMove,
	float upMove,
	std::uint16_t buttons,
	std::uint8_t impulse,
	std::uint8_t msec)
{
	GameDllRunPlayerMovePlan plan = {};

	if (!hasSpawnedClient)
	{
		plan.action = GameDllRunPlayerMoveAction::SkipMissingSpawnedClient;
		return plan;
	}

	if (!fakeClient)
	{
		plan.action = GameDllRunPlayerMoveAction::SkipRealClient;
		return plan;
	}

	plan.action = GameDllRunPlayerMoveAction::RunFakeClientMove;
	plan.timebase = (serverTime + frameTime) -
		(static_cast<double>(msec) / 1000.0);
	plan.command.viewAngles = viewAngles;
	plan.command.forwardMove = forwardMove;
	plan.command.sideMove = sideMove;
	plan.command.upMove = upMove;
	plan.command.buttons = buttons;
	plan.command.impulse = impulse;
	plan.command.msec = msec;
	return plan;
}

std::string BuildGameDllFakeClientName(const char *netname)
{
	if (!netname || netname[0] == '\0')
		return "Bot";

	return std::string(netname);
}

const char *GameDllMoveToOriginActionName(GameDllMoveToOriginAction action)
{
	switch (action)
	{
	case GameDllMoveToOriginAction::SkipMissingGoal:
		return "skip-missing-goal";
	case GameDllMoveToOriginAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllMoveToOriginAction::SkipImmobileEntity:
		return "skip-immobile-entity";
	case GameDllMoveToOriginAction::StepTowardIdealYaw:
		return "step-toward-ideal-yaw";
	case GameDllMoveToOriginAction::FlyDirectionTowardGoal:
		return "fly-direction-toward-goal";
	}

	return "unknown";
}

const char *GameDllAngularMoveActionName(GameDllAngularMoveAction action)
{
	switch (action)
	{
	case GameDllAngularMoveAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllAngularMoveAction::ApplyAngle:
		return "apply-angle";
	}

	return "unknown";
}

const char *GameDllWalkMoveActionName(GameDllWalkMoveAction action)
{
	switch (action)
	{
	case GameDllWalkMoveAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllWalkMoveAction::SkipImmobileEntity:
		return "skip-immobile-entity";
	case GameDllWalkMoveAction::MoveStepRelink:
		return "move-step-relink";
	case GameDllWalkMoveAction::MoveTestWorldOnly:
		return "move-test-world-only";
	case GameDllWalkMoveAction::MoveStepCheckOnly:
		return "move-step-check-only";
	case GameDllWalkMoveAction::FatalUnknownMode:
		return "fatal-unknown-mode";
	}

	return "unknown";
}

const char *GameDllSetOriginActionName(GameDllSetOriginAction action)
{
	switch (action)
	{
	case GameDllSetOriginAction::SkipInvalidEntity:
		return "skip-invalid-entity";
	case GameDllSetOriginAction::CopyOriginAndRelink:
		return "copy-origin-and-relink";
	}

	return "unknown";
}

const char *GameDllClientMaxspeedActionName(
	GameDllClientMaxspeedAction action)
{
	switch (action)
	{
	case GameDllClientMaxspeedAction::SkipMissingClient:
		return "skip-missing-client";
	case GameDllClientMaxspeedAction::SetMaxspeed:
		return "set-maxspeed";
	}

	return "unknown";
}

const char *GameDllRunPlayerMoveActionName(GameDllRunPlayerMoveAction action)
{
	switch (action)
	{
	case GameDllRunPlayerMoveAction::SkipMissingSpawnedClient:
		return "skip-missing-spawned-client";
	case GameDllRunPlayerMoveAction::SkipRealClient:
		return "skip-real-client";
	case GameDllRunPlayerMoveAction::RunFakeClientMove:
		return "run-fake-client-move";
	}

	return "unknown";
}

}
}
}
