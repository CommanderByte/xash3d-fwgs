#ifndef XASH_ENGINE_SERVER_SERVER_HOT_RESOURCE_ADAPTER_H
#define XASH_ENGINE_SERVER_SERVER_HOT_RESOURCE_ADAPTER_H

#include "custom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sv_hot_resource_file_size_query_s
{
	int should_announce;
	int needs_file_size;
	char file_size_path[MAX_VA_STRING];
} sv_hot_resource_file_size_query_t;

typedef struct sv_hot_resource_entry_s
{
	int should_announce;
	resourcetype_t type;
	const char *name;
	int index;
	int download_size;
	unsigned char flags;
} sv_hot_resource_entry_t;

sv_hot_resource_file_size_query_t SV_HotResource_BuildFileSizeQuery(
	const char *name,
	resourcetype_t type);
sv_hot_resource_entry_t SV_HotResource_BuildAnnouncement(
	const char *name,
	resourcetype_t type,
	int index,
	unsigned char flags,
	int probed_download_size);

#ifdef __cplusplus
}
#endif

#endif
