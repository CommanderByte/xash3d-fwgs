#ifndef XASH_ENGINE_SERVER_GROUP_FILTER_ADAPTER_H
#define XASH_ENGINE_SERVER_GROUP_FILTER_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int SV_GroupFilter_EntityPairPasses(
	int group_op,
	unsigned int left_group,
	unsigned int right_group);
int SV_GroupFilter_EntityPassesMask(
	int group_op,
	unsigned int entity_group,
	unsigned int group_mask);

#ifdef __cplusplus
}
#endif

#endif
