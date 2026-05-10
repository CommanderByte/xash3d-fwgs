#ifndef XASH_ENGINE_SERVER_SERVER_RESLIST_POLICY_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_RESLIST_POLICY_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sv_reslist_route_e
{
	SV_RESLIST_ROUTE_SKIP = 0,
	SV_RESLIST_ROUTE_SOUND_INDEX = 1,
	SV_RESLIST_ROUTE_GENERIC_INDEX = 2,
};

typedef struct sv_reslist_decision_s
{
	int should_index;
	resourcetype_t type;
	int route;
	char normalized_path[MAX_STRING];
	char index_path[MAX_STRING];
} sv_reslist_decision_t;

sv_reslist_decision_t SV_ReslistPolicy_ClassifyToken(const char *token);

#ifdef __cplusplus
}
#endif

#endif
