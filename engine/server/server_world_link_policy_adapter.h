#ifndef XASH_ENGINE_SERVER_WORLD_LINK_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_WORLD_LINK_POLICY_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SV_WORLD_AREA_CHILD_NONE (-1)
#define SV_WORLD_AREA_CHILD_POSITIVE 0
#define SV_WORLD_AREA_CHILD_NEGATIVE 1
#define SV_WORLD_AREA_CHILD_POSITIVE_MASK (1 << 0)
#define SV_WORLD_AREA_CHILD_NEGATIVE_MASK (1 << 1)

int SV_WorldArea_SelectSplitAxis(float size_x, float size_y);
float SV_WorldArea_BuildSplitDistance(float min, float max);
int SV_WorldArea_SelectLinkChild(float min, float max, float split_distance);
int SV_WorldArea_BuildTraversalMask(float min, float max, float split_distance);

#ifdef __cplusplus
}
#endif

#endif
