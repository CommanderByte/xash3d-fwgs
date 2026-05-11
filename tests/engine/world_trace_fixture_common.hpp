#ifndef XASH_TEST_ENGINE_WORLD_TRACE_FIXTURE_COMMON_HPP
#define XASH_TEST_ENGINE_WORLD_TRACE_FIXTURE_COMMON_HPP

#include "engine/server/shared/server_group_filter.hpp"
#include "engine/server/world/server_world_link_policy.hpp"
#include "engine/server/world/server_world_trace_policy.hpp"

namespace xash
{
namespace test
{
namespace engine
{

struct WorldFixtureVec3
{
	float x;
	float y;
	float z;
};

struct WorldFixtureBounds
{
	WorldFixtureVec3 mins;
	WorldFixtureVec3 maxs;
};

struct WorldFixtureAreaNode
{
	int axis;
	float splitDistance;
};

struct WorldFixtureEdict
{
	WorldFixtureBounds bounds;
	unsigned int groupInfo;
	bool free;
	bool trigger;
	bool solid;
	bool hasTouchCallback;
};

struct WorldFixtureMove
{
	WorldFixtureBounds sweptBounds;
	bool passedEntityValid;
	unsigned int passedEntityGroupInfo;
};

inline WorldFixtureVec3 FixtureVec3(float x, float y, float z)
{
	WorldFixtureVec3 value = {};
	value.x = x;
	value.y = y;
	value.z = z;
	return value;
}

inline WorldFixtureBounds FixtureBounds(
	const WorldFixtureVec3 &mins,
	const WorldFixtureVec3 &maxs)
{
	WorldFixtureBounds bounds = {};
	bounds.mins = mins;
	bounds.maxs = maxs;
	return bounds;
}

inline float FixtureAxisValue(const WorldFixtureVec3 &value, int axis)
{
	switch (axis)
	{
	case 0:
		return value.x;
	case 1:
		return value.y;
	default:
		return value.z;
	}
}

inline float FixtureAxisMin(const WorldFixtureBounds &bounds, int axis)
{
	return FixtureAxisValue(bounds.mins, axis);
}

inline float FixtureAxisMax(const WorldFixtureBounds &bounds, int axis)
{
	return FixtureAxisValue(bounds.maxs, axis);
}

inline bool FixtureBoundsIntersect(
	const WorldFixtureBounds &left,
	const WorldFixtureBounds &right)
{
	return left.mins.x <= right.maxs.x &&
		left.maxs.x >= right.mins.x &&
		left.mins.y <= right.maxs.y &&
		left.maxs.y >= right.mins.y &&
		left.mins.z <= right.maxs.z &&
		left.maxs.z >= right.mins.z;
}

inline WorldFixtureAreaNode FixtureRootAreaNode(
	const WorldFixtureBounds &worldBounds)
{
	WorldFixtureAreaNode node = {};
	const float sizeX = worldBounds.maxs.x - worldBounds.mins.x;
	const float sizeY = worldBounds.maxs.y - worldBounds.mins.y;
	node.axis = xash::engine::server::SelectWorldAreaSplitAxis(sizeX, sizeY);
	node.splitDistance = xash::engine::server::BuildWorldAreaSplitDistance(
		FixtureAxisMin(worldBounds, node.axis),
		FixtureAxisMax(worldBounds, node.axis));
	return node;
}

inline int FixtureLinkChild(
	const WorldFixtureAreaNode &node,
	const WorldFixtureBounds &bounds)
{
	return xash::engine::server::SelectWorldAreaLinkChild(
		FixtureAxisMin(bounds, node.axis),
		FixtureAxisMax(bounds, node.axis),
		node.splitDistance);
}

inline int FixtureTraversalMask(
	const WorldFixtureAreaNode &node,
	const WorldFixtureBounds &bounds)
{
	return xash::engine::server::BuildWorldAreaTraversalMask(
		FixtureAxisMin(bounds, node.axis),
		FixtureAxisMax(bounds, node.axis),
		node.splitDistance);
}

inline xash::engine::server::ServerWorldMoveClipPlan FixtureMoveClipPlan(
	int encodedMoveType,
	bool requestedMonsterClip,
	bool quakeCompatible,
	int missileMoveType)
{
	return xash::engine::server::BuildServerWorldMoveClipPlan(
		encodedMoveType,
		requestedMonsterClip,
		quakeCompatible,
		missileMoveType);
}

inline bool FixtureTouchCandidatePasses(
	const WorldFixtureEdict &moving,
	const WorldFixtureEdict &candidate,
	int legacyGroupOperation)
{
	if (moving.free || candidate.free || !candidate.trigger ||
		!candidate.hasTouchCallback)
	{
		return false;
	}

	if (!FixtureBoundsIntersect(moving.bounds, candidate.bounds))
		return false;

	return xash::engine::server::EntityPairPassesGroupFilter(
		legacyGroupOperation,
		candidate.groupInfo,
		moving.groupInfo);
}

inline bool FixtureClipCandidatePasses(
	const WorldFixtureMove &move,
	const WorldFixtureEdict &candidate,
	int legacyGroupOperation)
{
	if (candidate.free || !candidate.solid || candidate.trigger)
		return false;

	if (candidate.groupInfo != 0u && move.passedEntityValid &&
		!xash::engine::server::EntityPairPassesGroupFilter(
			legacyGroupOperation,
			candidate.groupInfo,
			move.passedEntityGroupInfo))
	{
		return false;
	}

	return FixtureBoundsIntersect(move.sweptBounds, candidate.bounds);
}

}
}
}

#endif
