#ifndef XASH_FILESYSTEM_SEARCHPATH_MOUNT_ADAPTER_H
#define XASH_FILESYSTEM_SEARCHPATH_MOUNT_ADAPTER_H

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct fs_searchpath_callbacks_s
{
	void ( *printInfo )( searchpath_t *search, char *dst, size_t size );
	void ( *close )( searchpath_t *search );
	file_t *( *openFile )( searchpath_t *search, const char *filename,
		const char *mode, int packIndex );
	int ( *fileTime )( searchpath_t *search, const char *filename );
	int ( *findFile )( searchpath_t *search, const char *path,
		char *fixedName, size_t length );
	void ( *search )( searchpath_t *search, stringlist_t *list,
		const char *pattern, int caseInsensitive );
	byte *( *loadFile )( searchpath_t *search, const char *path,
		int packIndex, fs_offset_t *fileSize, void *( *alloc )( size_t ),
		void ( *free )( void * ));
} fs_searchpath_callbacks_t;

searchpath_t *FS_SearchPath_Alloc(void);
void FS_SearchPath_Free(searchpath_t *path);
void FS_SearchPath_Init(searchpath_t *path, const char *filename,
	searchpathtype_t type, int flags,
	const fs_searchpath_callbacks_t *callbacks);

#ifdef __cplusplus
}
#endif

#endif
