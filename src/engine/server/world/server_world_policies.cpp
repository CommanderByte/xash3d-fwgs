#include "engine/server/world/server_world_link_policy.hpp"
#include "engine/server/world/server_world_trace_policy.hpp"
#include "engine/server/world/server_movement_constraints.hpp"
#include "engine/server/world/server_physics_routing_policy.hpp"
#include "engine/server/world/server_pmove_bridge_policy.hpp"

#include <cmath>

namespace xash
{
namespace engine
{
namespace server
{
namespace
{

constexpr int kLegacyMoveTypeNone = 0;
constexpr int kLegacyMoveTypeWalk = 3;
constexpr int kLegacyMoveTypeStep = 4;
constexpr int kLegacyMoveTypeFly = 5;
constexpr int kLegacyMoveTypeToss = 6;
constexpr int kLegacyMoveTypePush = 7;
constexpr int kLegacyMoveTypeNoclip = 8;
constexpr int kLegacyMoveTypeFlyMissile = 9;
constexpr int kLegacyMoveTypeBounce = 10;
constexpr int kLegacyMoveTypeBounceMissile = 11;
constexpr int kLegacyMoveTypeFollow = 12;
constexpr int kLegacyMoveTypePushStep = 13;
constexpr int kLegacyMoveTypeCompound = 14;

float MinFloat(float left, float right)
{
	return left < right ? left : right;
}

float Clamp01(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}

}

int SelectWorldAreaSplitAxis(float sizeX, float sizeY)
{
	return sizeX > sizeY ? 0 : 1;
}

float BuildWorldAreaSplitDistance(float min, float max)
{
	return 0.5f * (min + max);
}

int SelectWorldAreaLinkChild(float min, float max, float splitDistance)
{
	if (min > splitDistance)
		return kWorldAreaChildPositive;

	if (max < splitDistance)
		return kWorldAreaChildNegative;

	return kWorldAreaChildNone;
}

int BuildWorldAreaTraversalMask(float min, float max, float splitDistance)
{
	int mask = 0;

	if (max > splitDistance)
		mask |= kWorldAreaChildPositiveMask;

	if (min < splitDistance)
		mask |= kWorldAreaChildNegativeMask;

	return mask;
}

ServerWorldMoveClipPlan BuildServerWorldMoveClipPlan(
	int encodedMoveType,
	bool requestedMonsterClip,
	bool quakeCompatible,
	int missileMoveType)
{
	ServerWorldMoveClipPlan plan = {};
	plan.moveType = encodedMoveType & 0xff;
	plan.ignoreTransparent = encodedMoveType >> 8;
	plan.monsterClip = requestedMonsterClip && !quakeCompatible;
	plan.useMissileBounds = plan.moveType == missileMoveType;
	return plan;
}

int ToLegacyMonsterMoveType(ServerMonsterMoveType type)
{
	return static_cast<int>(type);
}

bool IsServerMonsterNormalMoveType(int legacyMoveType)
{
	return legacyMoveType == kServerMoveNormal;
}

bool IsServerMonsterStrafeMoveType(int legacyMoveType)
{
	return legacyMoveType == kServerMoveStrafe;
}

int ServerFlyMoveClipIterationLimit()
{
	return kServerMaxClipPlanes - 1;
}

bool ServerFlyMoveCanAddClipPlane(int planeCount)
{
	return planeCount >= 0 && planeCount < kServerMaxClipPlanes;
}

ServerMovementConstraintSnapshot BuildServerMovementConstraintSnapshot()
{
	ServerMovementConstraintSnapshot snapshot = {};
	snapshot.monsterMoveNormal = kServerMoveNormal;
	snapshot.monsterMoveStrafe = kServerMoveStrafe;
	snapshot.movementEpsilon = kServerMoveEpsilon;
	snapshot.flyMoveClipPlaneLimit = kServerMaxClipPlanes;
	snapshot.flyMoveClipIterationLimit = ServerFlyMoveClipIterationLimit();
	return snapshot;
}

ServerPhysicsHandler SelectServerPhysicsHandlerForMoveType(int legacyMoveType)
{
	switch (legacyMoveType)
	{
	case kLegacyMoveTypeNone:
		return ServerPhysicsHandler::None;
	case kLegacyMoveTypeNoclip:
		return ServerPhysicsHandler::Noclip;
	case kLegacyMoveTypeFollow:
		return ServerPhysicsHandler::Follow;
	case kLegacyMoveTypeCompound:
		return ServerPhysicsHandler::Compound;
	case kLegacyMoveTypeStep:
	case kLegacyMoveTypePushStep:
		return ServerPhysicsHandler::Step;
	case kLegacyMoveTypeFly:
	case kLegacyMoveTypeToss:
	case kLegacyMoveTypeBounce:
	case kLegacyMoveTypeFlyMissile:
	case kLegacyMoveTypeBounceMissile:
		return ServerPhysicsHandler::Toss;
	case kLegacyMoveTypePush:
		return ServerPhysicsHandler::Pusher;
	case kLegacyMoveTypeWalk:
		return ServerPhysicsHandler::InvalidWalk;
	default:
		return ServerPhysicsHandler::Unsupported;
	}
}

bool PusherConsidersMoveType(int legacyMoveType)
{
	switch (legacyMoveType)
	{
	case kLegacyMoveTypeNone:
	case kLegacyMoveTypePush:
	case kLegacyMoveTypeFollow:
	case kLegacyMoveTypeNoclip:
	case kLegacyMoveTypeCompound:
		return false;
	default:
		return true;
	}
}

bool PushedEntityUsesPreciseBlocking(int legacyMoveType)
{
	return legacyMoveType == kLegacyMoveTypeWalk ||
		legacyMoveType == kLegacyMoveTypeStep ||
		legacyMoveType == kLegacyMoveTypePushStep;
}

bool ShouldEnablePmoveUnlag(const PmoveUnlagAdmissionFacts &facts)
{
	if (facts.maxClients <= 1)
		return false;
	if (!facts.gameAllowsLagCompensation)
		return false;
	if (!facts.serverUnlagEnabled)
		return false;
	if (!facts.clientLagCompensationEnabled)
		return false;
	return facts.clientSpawned;
}

bool IsPmovePlayerEntityIndex(int edictIndex, int maxClients)
{
	return edictIndex >= 1 && edictIndex <= maxClients;
}

bool ShouldUsePmoveInterpolatedPlayer(
	int edictIndex,
	int maxClients,
	bool interpolantActive,
	bool interpolantMoving)
{
	return IsPmovePlayerEntityIndex(edictIndex, maxClients) &&
		interpolantActive &&
		interpolantMoving;
}

bool IsPmoveUnlagTeleport(
	const PmoveVector3 &oldPosition,
	const PmoveVector3 &newPosition)
{
	return std::fabs(oldPosition.x - newPosition.x) >
			kPmoveUnlagTeleportDistance ||
		std::fabs(oldPosition.y - newPosition.y) >
			kPmoveUnlagTeleportDistance ||
		std::fabs(oldPosition.z - newPosition.z) >
			kPmoveUnlagTeleportDistance;
}

PmoveUnlagLatencyPlan BuildPmoveUnlagLatencyPlan(
	float clientLatency,
	float maxUnlag)
{
	PmoveUnlagLatencyPlan plan = {};
	plan.normalizedMaxUnlag = maxUnlag;
	plan.latency = MinFloat(clientLatency, kPmoveUnlagMaxLatencySeconds);

	if (maxUnlag != 0.0f)
	{
		if (maxUnlag < 0.0f)
		{
			plan.normalizedMaxUnlag = 0.0f;
			plan.clampMaxUnlagCvarToZero = true;
		}

		plan.latency = MinFloat(plan.latency, plan.normalizedMaxUnlag);
	}

	return plan;
}

float BuildPmoveLerpSeconds(int lerpMilliseconds, float nextMessageInterval)
{
	float lerpSeconds = static_cast<float>(lerpMilliseconds) * 0.001f;

	if (lerpSeconds > kPmoveUnlagMaxLerpSeconds)
		lerpSeconds = kPmoveUnlagMaxLerpSeconds;
	if (lerpSeconds < nextMessageInterval)
		lerpSeconds = nextMessageInterval;

	return lerpSeconds;
}

float BuildPmoveUnlagTargetTime(
	float realtime,
	float latency,
	float lerpSeconds,
	float pushSeconds)
{
	float targetTime = (realtime - latency - lerpSeconds) + pushSeconds;
	if (targetTime > realtime)
		targetTime = realtime;
	return targetTime;
}

float BuildPmoveInterpolationFraction(
	float targetTime,
	float frameTime,
	float nextFrameTime)
{
	const float span = nextFrameTime - frameTime;
	if (span == 0.0f)
		return 0.0f;
	return Clamp01((targetTime - frameTime) / span);
}

}
}
}
