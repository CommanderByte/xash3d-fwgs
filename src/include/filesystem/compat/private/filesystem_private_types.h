#ifndef XASH_FILESYSTEM_PRIVATE_TYPES_H
#define XASH_FILESYSTEM_PRIVATE_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "xash3d_types.h"
#include "filesystem.h"
#include "miniz.h"

#if XASH_ANDROID
#include <android/asset_manager.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct dir_s dir_t;
typedef struct zip_s zip_t;
typedef struct pack_s pack_t;
typedef struct wfile_s wfile_t;
typedef struct android_assets_s android_assets_t;

#define FILE_BUFF_SIZE (2048)
#define FILE_DEFLATED BIT( 0 )

typedef struct ztoolkit_s
{
	z_stream zstream;
	size_t   comp_length;
	size_t   in_ind, in_len;
	size_t   in_position;
	byte     input[FILE_BUFF_SIZE];
} ztoolkit_t;

struct file_s
{
	int          handle;      // file descriptor
	int          ungetc;      // single stored character from ungetc, cleared to EOF when read
	time_t       filetime;    // pak, wad or real filetime
	searchpath_t *searchpath;
	fs_offset_t  real_length; // uncompressed file size (for files opened in "read" mode)
	fs_offset_t  position;    // current position in the file
	fs_offset_t  offset;      // offset into the package (0 if external file)
	uint32_t     flags;
	ztoolkit_t   *ztk; // if not NULL, all read functions must go through decompression

	// contents buffer
	fs_offset_t buff_ind; // buffer current index
	fs_offset_t buff_len; // buffer current length
	byte         buff[FILE_BUFF_SIZE]; // intermediate buffer

#ifdef XASH_REDUCE_FD
	const char *backup_path;
	fs_offset_t backup_position;
	uint backup_options;
#endif
};

typedef enum searchpathtype_e
{
	SEARCHPATH_PLAIN = 0,
	SEARCHPATH_PAK,
	SEARCHPATH_WAD,
	SEARCHPATH_ZIP,
	SEARCHPATH_PK3DIR, // it's actually a plain directory but it must behave like a ZIP archive,
	SEARCHPATH_ANDROID_ASSETS
} searchpathtype_t;

typedef struct stringlist_s
{
	// maxstrings changes as needed, causing reallocation of strings[] array
	int   maxstrings;
	int   numstrings;
	char **strings;
} stringlist_t;

typedef struct searchpath_s
{
	string           filename;
	searchpathtype_t type;
	int              flags;

	union
	{
		dir_t            *dir;
		pack_t           *pack;
		wfile_t          *wad;
		zip_t            *zip;
		android_assets_t *assets;
	};

	struct searchpath_s *next;

	void    ( *pfnPrintInfo )( struct searchpath_s *search, char *dst, size_t size );
	void    ( *pfnClose )( struct searchpath_s *search );
	file_t *( *pfnOpenFile )( struct searchpath_s *search, const char *filename, const char *mode, int pack_ind );
	int     ( *pfnFileTime )( struct searchpath_s *search, const char *filename );
	int     ( *pfnFindFile )( struct searchpath_s *search, const char *path, char *fixedname, size_t len );
	void    ( *pfnSearch )( struct searchpath_s *search, stringlist_t *list, const char *pattern, int caseinsensitive );
	byte   *( *pfnLoadFile )( struct searchpath_s *search, const char *path, int pack_ind, fs_offset_t *filesize, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ));
} searchpath_t;

typedef searchpath_t *( *FS_ADDARCHIVE_FULLPATH )( const char *path, int flags );

#ifdef __cplusplus
}
#endif

#endif
