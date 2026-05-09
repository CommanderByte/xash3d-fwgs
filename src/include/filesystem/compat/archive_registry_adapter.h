#ifndef FILESYSTEM_ARCHIVE_REGISTRY_ADAPTER_H
#define FILESYSTEM_ARCHIVE_REGISTRY_ADAPTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct fs_archive_registry_entry_s
{
	const char *extension;
	int searchpath_type;
	int real_archive;
	int auto_mount_contained_wads;
	int scan_priority;
	const char *debug_name;
	const char *factory_name;
} fs_archive_registry_entry_t;

size_t FS_ArchiveRegistry_Count( void );
int FS_ArchiveRegistry_EntryAt( size_t index, fs_archive_registry_entry_t *entry );
int FS_ArchiveRegistry_Find( const char *extension, int only_real_archives, fs_archive_registry_entry_t *entry );

#ifdef __cplusplus
}
#endif

#endif // FILESYSTEM_ARCHIVE_REGISTRY_ADAPTER_H
