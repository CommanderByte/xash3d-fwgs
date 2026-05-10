#ifndef XASH_ENGINE_SERVER_RESOURCE_CATALOG_ADAPTER_H
#define XASH_ENGINE_SERVER_RESOURCE_CATALOG_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_resource_catalog_state_s
{
	int sound_sentence_marker_added;
} sv_resource_catalog_state_t;

typedef struct sv_resource_catalog_entry_s
{
	int should_add;
	resourcetype_t type;
	const char *name;
	int download_size;
	unsigned char flags;
	int index;
} sv_resource_catalog_entry_t;

void SV_ResourceCatalog_Init(sv_resource_catalog_state_t *state);
int SV_ResourceCatalog_NeedsFileSize(resourcetype_t type, const char *name);
sv_resource_catalog_entry_t SV_ResourceCatalog_AddGeneric(
	const char *name,
	int index,
	int probed_download_size);
sv_resource_catalog_entry_t SV_ResourceCatalog_AddSound(
	sv_resource_catalog_state_t *state,
	const char *name,
	int index,
	int probed_download_size);
sv_resource_catalog_entry_t SV_ResourceCatalog_AddModel(
	const char *name,
	int index,
	int probed_download_size,
	unsigned char flags);
sv_resource_catalog_entry_t SV_ResourceCatalog_AddDecal(const char *name, int index);
sv_resource_catalog_entry_t SV_ResourceCatalog_AddEventScript(
	const char *name,
	int index,
	int probed_download_size);

#ifdef __cplusplus
}
#endif

#endif
