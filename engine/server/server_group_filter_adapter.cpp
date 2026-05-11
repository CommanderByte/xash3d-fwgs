#include "server_group_filter_adapter.h"

#include "engine/server/server_group_filter.hpp"

extern "C" int SV_GroupFilter_EntityPairPasses(
	int group_op,
	unsigned int left_group,
	unsigned int right_group)
{
	return xash::engine::server::EntityPairPassesGroupFilter(
		group_op,
		left_group,
		right_group) ? 1 : 0;
}

extern "C" int SV_GroupFilter_EntityPassesMask(
	int group_op,
	unsigned int entity_group,
	unsigned int group_mask)
{
	return xash::engine::server::EntityPassesGroupMask(
		group_op,
		entity_group,
		group_mask) ? 1 : 0;
}
