#include <cstdlib>

#include "engine/server/world/server_world_link_policy.hpp"
#include "world_trace_fixture_common.hpp"

using namespace xash::engine::server;
using namespace xash::test::engine;

namespace
{

namespace legacy
{

constexpr int kGroupOpAnd = 0;
constexpr int kGroupOpNand = 1;

}

static WorldFixtureBounds Box(
	float minX,
	float minY,
	float minZ,
	float maxX,
	float maxY,
	float maxZ)
{
	return FixtureBounds(
		FixtureVec3(minX, minY, minZ),
		FixtureVec3(maxX, maxY, maxZ));
}

static WorldFixtureEdict Edict(
	const WorldFixtureBounds &bounds,
	unsigned int groupInfo,
	bool trigger,
	bool solid)
{
	WorldFixtureEdict edict = {};
	edict.bounds = bounds;
	edict.groupInfo = groupInfo;
	edict.trigger = trigger;
	edict.solid = solid;
	edict.hasTouchCallback = true;
	return edict;
}

static bool TestAreaNodeFixtureUsesModernSplitPolicy()
{
	const WorldFixtureAreaNode wideX = FixtureRootAreaNode(
		Box(-1024.0f, -128.0f, -64.0f, 1024.0f, 128.0f, 64.0f));
	const WorldFixtureAreaNode wideY = FixtureRootAreaNode(
		Box(-128.0f, -512.0f, -64.0f, 128.0f, 512.0f, 64.0f));

	return wideX.axis == 0 &&
		wideX.splitDistance == 0.0f &&
		wideY.axis == 1 &&
		wideY.splitDistance == 0.0f;
}

static bool TestFixtureModelsLinkChildSelection()
{
	const WorldFixtureAreaNode node =
		FixtureRootAreaNode(Box(-64.0f, -16.0f, 0.0f, 64.0f, 16.0f, 16.0f));

	return FixtureLinkChild(node, Box(2.0f, -4.0f, 0.0f, 8.0f, 4.0f, 4.0f)) ==
			kWorldAreaChildPositive &&
		FixtureLinkChild(node, Box(-8.0f, -4.0f, 0.0f, -2.0f, 4.0f, 4.0f)) ==
			kWorldAreaChildNegative &&
		FixtureLinkChild(node, Box(-2.0f, -4.0f, 0.0f, 2.0f, 4.0f, 4.0f)) ==
			kWorldAreaChildNone;
}

static bool TestFixtureModelsTraversalMaskSelection()
{
	const WorldFixtureAreaNode node =
		FixtureRootAreaNode(Box(-64.0f, -16.0f, 0.0f, 64.0f, 16.0f, 16.0f));

	return FixtureTraversalMask(node, Box(2.0f, -4.0f, 0.0f, 8.0f, 4.0f, 4.0f)) ==
			kWorldAreaChildPositiveMask &&
		FixtureTraversalMask(node, Box(-8.0f, -4.0f, 0.0f, -2.0f, 4.0f, 4.0f)) ==
			kWorldAreaChildNegativeMask &&
		FixtureTraversalMask(node, Box(-2.0f, -4.0f, 0.0f, 2.0f, 4.0f, 4.0f)) ==
			(kWorldAreaChildPositiveMask | kWorldAreaChildNegativeMask);
}

static bool TestFixtureModelsMoveClipPlanSelection()
{
	constexpr int kMoveNormal = 1;
	constexpr int kMoveMissile = 3;

	const ServerWorldMoveClipPlan normal =
		FixtureMoveClipPlan((2 << 8) | kMoveNormal, true, false, kMoveMissile);
	const ServerWorldMoveClipPlan quake =
		FixtureMoveClipPlan((2 << 8) | kMoveNormal, true, true, kMoveMissile);
	const ServerWorldMoveClipPlan missile =
		FixtureMoveClipPlan((5 << 8) | kMoveMissile, false, false, kMoveMissile);

	return normal.moveType == kMoveNormal &&
		normal.ignoreTransparent == 2 &&
		normal.monsterClip &&
		!normal.useMissileBounds &&
		!quake.monsterClip &&
		missile.moveType == kMoveMissile &&
		missile.ignoreTransparent == 5 &&
		missile.useMissileBounds;
}

static bool TestFixtureModelsTriggerTouchAdmission()
{
	const WorldFixtureEdict moving =
		Edict(Box(-1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 2.0f), 0x01u, false, true);
	WorldFixtureEdict trigger =
		Edict(Box(0.0f, 0.0f, 0.0f, 3.0f, 3.0f, 2.0f), 0x01u, true, false);
	WorldFixtureEdict filtered =
		Edict(Box(0.0f, 0.0f, 0.0f, 3.0f, 3.0f, 2.0f), 0x02u, true, false);
	WorldFixtureEdict far =
		Edict(Box(8.0f, 8.0f, 0.0f, 10.0f, 10.0f, 2.0f), 0x01u, true, false);

	trigger.hasTouchCallback = true;
	filtered.hasTouchCallback = true;
	far.hasTouchCallback = true;

	return FixtureTouchCandidatePasses(moving, trigger, legacy::kGroupOpAnd) &&
		!FixtureTouchCandidatePasses(moving, filtered, legacy::kGroupOpAnd) &&
		FixtureTouchCandidatePasses(moving, filtered, legacy::kGroupOpNand) &&
		!FixtureTouchCandidatePasses(moving, far, legacy::kGroupOpAnd);
}

static bool TestFixtureModelsClipAdmissionWithoutEngineRuntime()
{
	WorldFixtureMove move = {};
	move.sweptBounds = Box(-4.0f, -4.0f, 0.0f, 4.0f, 4.0f, 4.0f);
	move.passedEntityValid = true;
	move.passedEntityGroupInfo = 0x01u;

	const WorldFixtureEdict solid =
		Edict(Box(2.0f, 2.0f, 0.0f, 6.0f, 6.0f, 4.0f), 0x01u, false, true);
	const WorldFixtureEdict groupFiltered =
		Edict(Box(2.0f, 2.0f, 0.0f, 6.0f, 6.0f, 4.0f), 0x02u, false, true);
	const WorldFixtureEdict trigger =
		Edict(Box(2.0f, 2.0f, 0.0f, 6.0f, 6.0f, 4.0f), 0x01u, true, false);
	const WorldFixtureEdict far =
		Edict(Box(16.0f, 16.0f, 0.0f, 20.0f, 20.0f, 4.0f), 0x01u, false, true);

	return FixtureClipCandidatePasses(move, solid, legacy::kGroupOpAnd) &&
		!FixtureClipCandidatePasses(move, groupFiltered, legacy::kGroupOpAnd) &&
		FixtureClipCandidatePasses(move, groupFiltered, legacy::kGroupOpNand) &&
		!FixtureClipCandidatePasses(move, trigger, legacy::kGroupOpAnd) &&
		!FixtureClipCandidatePasses(move, far, legacy::kGroupOpAnd);
}

}

int main()
{
	if (!TestAreaNodeFixtureUsesModernSplitPolicy() ||
		!TestFixtureModelsLinkChildSelection() ||
		!TestFixtureModelsTraversalMaskSelection() ||
		!TestFixtureModelsMoveClipPlanSelection() ||
		!TestFixtureModelsTriggerTouchAdmission() ||
		!TestFixtureModelsClipAdmissionWithoutEngineRuntime())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
