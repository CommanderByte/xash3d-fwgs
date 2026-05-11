#include "server_world_link_policy_adapter.h"

#include "engine/server/server_world_link_policy.hpp"

static_assert(SV_WORLD_AREA_CHILD_NONE ==
	xash::engine::server::kWorldAreaChildNone,
	"world area no-child value changed");
static_assert(SV_WORLD_AREA_CHILD_POSITIVE ==
	xash::engine::server::kWorldAreaChildPositive,
	"world area positive child value changed");
static_assert(SV_WORLD_AREA_CHILD_NEGATIVE ==
	xash::engine::server::kWorldAreaChildNegative,
	"world area negative child value changed");
static_assert(SV_WORLD_AREA_CHILD_POSITIVE_MASK ==
	xash::engine::server::kWorldAreaChildPositiveMask,
	"world area positive child mask changed");
static_assert(SV_WORLD_AREA_CHILD_NEGATIVE_MASK ==
	xash::engine::server::kWorldAreaChildNegativeMask,
	"world area negative child mask changed");

extern "C" int SV_WorldArea_SelectSplitAxis(float size_x, float size_y)
{
	return xash::engine::server::SelectWorldAreaSplitAxis(size_x, size_y);
}

extern "C" float SV_WorldArea_BuildSplitDistance(float min, float max)
{
	return xash::engine::server::BuildWorldAreaSplitDistance(min, max);
}

extern "C" int SV_WorldArea_SelectLinkChild(
	float min,
	float max,
	float split_distance)
{
	return xash::engine::server::SelectWorldAreaLinkChild(
		min,
		max,
		split_distance);
}

extern "C" int SV_WorldArea_BuildTraversalMask(
	float min,
	float max,
	float split_distance)
{
	return xash::engine::server::BuildWorldAreaTraversalMask(
		min,
		max,
		split_distance);
}
