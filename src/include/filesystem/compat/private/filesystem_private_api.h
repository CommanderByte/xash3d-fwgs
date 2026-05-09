#ifndef XASH_FILESYSTEM_PRIVATE_API_H
#define XASH_FILESYSTEM_PRIVATE_API_H

#include <stdarg.h>

#include "filesystem/compat/private/filesystem_private_memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

//
// filesystem.c
//
qboolean FS_InitStdio( qboolean caseinsensitive, const char *rootdir, const char *basedir, const char *gamedir, const char *rodir );
void FS_ShutdownStdio( void );
searchpath_t *FS_MountArchive_Fullpath( const char *file, int flags );

// search path utils
void FS_Rescan( uint32_t flags, const char *language );
void FS_ClearSearchPath( void );
void FS_AllowDirectPaths( qboolean enable );
void FS_AddGameDirectory( const char *dir, uint flags );
void FS_AddGameHierarchy( const char *dir, uint flags );
search_t *FS_Search( const char *pattern, int caseinsensitive, int gamedironly )
	MALLOC_LIKE( _Mem_Free, 1 ) WARN_UNUSED_RESULT;
int FS_SetCurrentDirectory( const char *path );
qboolean FS_GetRootDirectory( char *path, size_t size );
void FS_Path_f( void );

// file ops
int FS_Close( file_t *file );
file_t *FS_Open( const char *filepath, const char *mode, qboolean gamedironly )
	MALLOC_LIKE( FS_Close, 1 ) WARN_UNUSED_RESULT;
fs_offset_t FS_Write( file_t *file, const void *data, size_t datasize );
fs_offset_t FS_Read( file_t *file, void *buffer, size_t buffersize );
int FS_Seek( file_t *file, fs_offset_t offset, int whence );
fs_offset_t FS_Tell( const file_t *file );
qboolean FS_Eof( const file_t *file );
int FS_Flush( file_t *file );
int FS_Gets( file_t *file, char *string, size_t bufsize );
int FS_UnGetc( file_t *file, char c );
int FS_Getc( file_t *file );
int FS_VPrintf( file_t *file, const char *format, va_list ap );
int FS_Printf( file_t *file, const char *format, ... ) FORMAT_CHECK( 2 );
int FS_Print( file_t *file, const char *msg );
fs_offset_t FS_FileLength( const file_t *f );
qboolean FS_FileCopy( file_t *pOutput, file_t *pInput, int fileSize );

// file buffer ops
byte *FS_LoadFile( const char *path, fs_offset_t *filesizeptr, qboolean gamedironly )
	MALLOC_LIKE( _Mem_Free, 1 ) WARN_UNUSED_RESULT;
byte *FS_LoadFileMalloc( const char *path, fs_offset_t *filesizeptr, qboolean gamedironly )
	MALLOC_LIKE( free, 1 ) WARN_UNUSED_RESULT;
byte *FS_LoadDirectFile( const char *path, fs_offset_t *filesizeptr )
	MALLOC_LIKE( _Mem_Free, 1 ) WARN_UNUSED_RESULT;
qboolean FS_WriteFile( const char *filename, const void *data, fs_offset_t len );

// file hashing
qboolean CRC32_File( dword *crcvalue, const char *filename );
qboolean MD5_HashFile( byte digest[16], const char *pszFileName, uint seed[4] );

// stringlist ops
void stringlistinit( stringlist_t *list );
void stringlistfreecontents( stringlist_t *list );
void stringlistappend( stringlist_t *list, const char *text );
void stringlistsort( stringlist_t *list );
void listdirectory( stringlist_t *list, const char *path, qboolean dirs_only );

// filesystem ops
int FS_FileExists( const char *filename, int gamedironly );
int FS_FileTime( const char *filename, qboolean gamedironly );
fs_offset_t FS_FileSize( const char *filename, qboolean gamedironly );
qboolean FS_Rename( const char *oldname, const char *newname );
qboolean FS_Delete( const char *path );
qboolean FS_SysFileExists( const char *path );
const char *FS_GetDiskPath( const char *name, qboolean gamedironly );
qboolean FS_GetFullDiskPath( char *buffer, size_t size, const char *name, qboolean gamedironly );
void FS_CreatePath( char *path );
qboolean FS_SysFolderExists( const char *path );
qboolean FS_SysFileOrFolderExists( const char *path );
file_t *FS_OpenReadFile( const char *filename, const char *mode, qboolean gamedironly );

int FS_SysFileTime( const char *filename );
file_t *FS_OpenHandle( searchpath_t *search, int handle, fs_offset_t offset, fs_offset_t len );
file_t *FS_SysOpen( const char *filepath, const char *mode );
searchpath_t *FS_FindFile( const char *name, int *index, char *fixedname, size_t len, qboolean gamedironly );
qboolean FS_FullPathToRelativePath( char *dst, const char *src, size_t size );

//
// pak.c
//
qboolean FS_CheckForQuakePak( const char *pakfile, const char *files[], size_t num_files );
searchpath_t *FS_AddPak_Fullpath( const char *pakfile, int flags );

//
// wad.c
//
searchpath_t *FS_AddWad_Fullpath( const char *wadfile, int flags );

//
// zip.c
//
searchpath_t *FS_AddZip_Fullpath( const char *zipfile, int flags );

//
// dir.c
//
searchpath_t *FS_AddDir_Fullpath( const char *path, int flags );
qboolean FS_FixFileCase( dir_t *dir, const char *path, char *dst, const size_t len, qboolean createpath );
void FS_InitDirectorySearchpath( searchpath_t *search, const char *path, int flags );

//
// android.c
//
void FS_InitAndroid( void );
searchpath_t *FS_AddAndroidAssets_Fullpath( const char *path, int flags );

#ifdef __cplusplus
}
#endif

#endif
