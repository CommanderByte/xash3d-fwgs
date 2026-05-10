#ifndef XASH_ENGINE_SERVER_VISIBILITY_CONSTRAINTS_ADAPTER_H
#define XASH_ENGINE_SERVER_VISIBILITY_CONSTRAINTS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

int SV_Visibility_EntityLeafCapacity(int extended_leafs);
int SV_Visibility_CanStoreEntityLeaf(
	int current_leaf_count,
	int extended_leafs);
int SV_Visibility_EntityLeafOverflowMarker(int extended_leafs);
int SV_Visibility_EntityLeafOverflowed(
	int current_leaf_count,
	int extended_leafs);
int SV_Visibility_NextCachedLeafIndex(
	int current_leaf_count,
	int extended_leafs);
int SV_Visibility_CanAddViewEntity(int current_view_entity_count);

#ifdef __cplusplus
}
#endif

#endif
