#include "server_visibility_constraints_adapter.h"

#include "engine/server/server_visibility_constraints.hpp"

extern "C" int SV_Visibility_EntityLeafCapacity(int extended_leafs)
{
	return xash::engine::server::ServerVisibilityEntityLeafCapacity(
		extended_leafs != 0);
}

extern "C" int SV_Visibility_CanStoreEntityLeaf(
	int current_leaf_count,
	int extended_leafs)
{
	return xash::engine::server::ServerVisibilityCanStoreEntityLeaf(
		current_leaf_count,
		extended_leafs != 0) ? 1 : 0;
}

extern "C" int SV_Visibility_EntityLeafOverflowMarker(int extended_leafs)
{
	return xash::engine::server::ServerVisibilityEntityLeafOverflowMarker(
		extended_leafs != 0);
}

extern "C" int SV_Visibility_EntityLeafOverflowed(
	int current_leaf_count,
	int extended_leafs)
{
	return xash::engine::server::ServerVisibilityEntityLeafOverflowed(
		current_leaf_count,
		extended_leafs != 0) ? 1 : 0;
}

extern "C" int SV_Visibility_NextCachedLeafIndex(
	int current_leaf_count,
	int extended_leafs)
{
	return xash::engine::server::ServerVisibilityNextCachedLeafIndex(
		current_leaf_count,
		extended_leafs != 0);
}

extern "C" int SV_Visibility_CanAddViewEntity(int current_view_entity_count)
{
	return xash::engine::server::ServerVisibilityCanAddViewEntity(
		current_view_entity_count) ? 1 : 0;
}
