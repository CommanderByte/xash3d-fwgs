/*
wad.c - WAD support for filesystem
Copyright (C) 2003-2006 Mathieu Olivier
Copyright (C) 2000-2007 DarkPlaces contributors
Copyright (C) 2007 Uncle Mike
Copyright (C) 2015-2023 Xash3D FWGS contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if XASH_POSIX
#include <unistd.h>
#endif
#include <errno.h>
#include <stddef.h>
#include "port.h"
#include "filesystem_internal.h"
#include "wad_backend_adapter.h"
#include "crtlib.h"
#include "common/com_strings.h"
#include "wadfile.h"

/*
========================================================================
.WAD archive format	(WhereAllData - WAD)

List of compressed files, that can be identify only by TYPE_*

<format>
header:	dwadinfo_t[dwadinfo_t]
file_1:	byte[dwadinfo_t[num]->disksize]
file_2:	byte[dwadinfo_t[num]->disksize]
file_3:	byte[dwadinfo_t[num]->disksize]
...
file_n:	byte[dwadinfo_t[num]->disksize]
infotable	dlumpinfo_t[dwadinfo_t->numlumps]
========================================================================
*/
#define HINT_NAMELEN	5	// e.g. _mask, _norm
struct wfile_s
{
	int		infotableofs;
	int		numlumps;
	poolhandle_t mempool;			// W_ReadLump temp buffers
	file_t		*handle;
	dlumpinfo_t	*lumps;
	time_t		filetime;
	void		*backend;
};

// WAD errors
#define WAD_LOAD_OK			0
#define WAD_LOAD_COULDNT_OPEN		1
#define WAD_LOAD_BAD_HEADER		2
#define WAD_LOAD_BAD_FOLDERS		3
#define WAD_LOAD_TOO_MANY_FILES	4
#define WAD_LOAD_NO_FILES		5
#define WAD_LOAD_CORRUPTED		6

/*
===========
FS_CloseWAD

finalize wad or just close
===========
*/
static void FS_CloseWAD( wfile_t *wad )
{
	Mem_FreePool( &wad->mempool );
	if( wad->handle != NULL )
		FS_Close( wad->handle );
	Mem_Free( wad ); // free himself
}

static fs_offset_t W_RuntimeRead( void *context, file_t *file, void *buffer, size_t size )
{
	(void)context;

	return FS_Read( file, buffer, size );
}

static int W_RuntimeSeek( void *context, file_t *file, fs_offset_t offset, int whence )
{
	(void)context;

	return FS_Seek( file, offset, whence );
}

static void *W_RuntimeAlloc( void *context, poolhandle_t pool, size_t size, int clear )
{
	(void)context;

	if( clear )
		return Mem_Calloc( pool, size );

	return Mem_Malloc( pool, size );
}

static void W_RuntimeFree( void *context, void *memory )
{
	(void)context;

	Mem_Free( memory );
}

static void W_RuntimeDuplicateLump( void *context, const char *wadfile, const char *name )
{
	(void)context;

	Con_Reportf( S_WARN "Wad %s contains the file %s several times\n", wadfile, name );
}

static file_t *W_OpenPackedFile( void *context, const char *filename )
{
	(void)context;

	return FS_Open( filename, "rb", false );
}

static file_t *W_OpenSystemFile( void *context, const char *filename, const char *mode )
{
	(void)context;

	return FS_SysOpen( filename, mode );
}

static int W_OpenFileTime( void *context, const char *filename )
{
	(void)context;

	return FS_SysFileTime( filename );
}

static poolhandle_t W_OpenAllocPool( void *context, const char *name )
{
	(void)context;

	return Mem_AllocPool( name );
}

static void W_OpenFreePool( void *context, poolhandle_t *pool )
{
	(void)context;

	Mem_FreePool( pool );
}

static void W_OpenCloseFile( void *context, file_t *file )
{
	(void)context;

	FS_Close( file );
}

static fs_wad_archive_view_t W_MakeArchiveView( searchpath_t *search )
{
	fs_wad_archive_view_t archive;

	archive.source = search->filename;
	archive.lumpCount = search->wad ? search->wad->numlumps : 0;
	archive.lumps = search->wad ? search->wad->lumps : NULL;
	archive.handle = search->wad ? search->wad->handle : NULL;
	archive.fileTime = search->wad ? search->wad->filetime : 0;

	return archive;
}

static int W_SearchMatchPattern( void *context, const char *text, const char *pattern, int caseinsensitive )
{
	(void)context;

	return matchpattern( text, pattern, caseinsensitive );
}

static int W_SearchStringCount( void *context, stringlist_t *list )
{
	(void)context;

	return list ? list->numstrings : 0;
}

static const char *W_SearchStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;

	if( !list || index < 0 || index >= list->numstrings )
		return NULL;

	return list->strings[index];
}

static void W_SearchAppend( void *context, stringlist_t *list, const char *text )
{
	(void)context;

	stringlistappend( list, text );
}

static fs_offset_t W_ReadTell( void *context, file_t *file )
{
	(void)context;

	return FS_Tell( file );
}

static int W_ReadSeek( void *context, file_t *file, fs_offset_t offset, int whence )
{
	(void)context;

	return FS_Seek( file, offset, whence );
}

static fs_offset_t W_ReadRead( void *context, file_t *file, void *buffer, size_t size )
{
	(void)context;

	return FS_Read( file, buffer, size );
}

static void W_ReadCorrupted( void *context, const char *name )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: %s is corrupted\n", __func__, name );
}

static void W_ReadAllocationFailed( void *context, size_t size )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: can't alloc %d bytes, no free memory\n", __func__, (int)size );
}

static void W_ReadShortRead( void *context, const char *name )
{
	(void)context;

	Con_Reportf( S_WARN "%s: %s is probably corrupted\n", __func__, name );
}

/*
===========
FS_Close_WAD
===========
*/
static void FS_Close_WAD_Legacy( searchpath_t *search )
{
	FS_CloseWAD( search->wad );
}

/*
===========
FS_OpenFile_WAD
===========
*/
static file_t *FS_OpenFile_WAD_Legacy( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	(void)search;
	(void)filename;
	(void)mode;
	(void)pack_ind;

	return NULL;
}

/*
===========
W_Open

open the wad for reading & writing
===========
*/
static wfile_t *W_Open( const char *filename, int *error, uint flags )
{
	wfile_t		*wad = (wfile_t *)Mem_Calloc( fs_mempool, sizeof( wfile_t ));
	fs_wad_open_runtime_t runtime;
	fs_wad_open_result_t result;
	int status;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.openPacked = W_OpenPackedFile;
	runtime.openSystem = W_OpenSystemFile;
	runtime.fileTime = W_OpenFileTime;
	runtime.allocPool = W_OpenAllocPool;
	runtime.freePool = W_OpenFreePool;
	runtime.close = W_OpenCloseFile;
	runtime.loadRuntime.read = W_RuntimeRead;
	runtime.loadRuntime.seek = W_RuntimeSeek;
	runtime.loadRuntime.alloc = W_RuntimeAlloc;
	runtime.loadRuntime.free = W_RuntimeFree;
	runtime.loadRuntime.duplicateLump = W_RuntimeDuplicateLump;

	memset( &result, 0, sizeof( result ));
	status = FS_WadBackend_OpenArchive( &runtime, filename, FBitSet( flags, FS_LOAD_PACKED_WAD ), &result );

	if( status == WAD_LOAD_COULDNT_OPEN )
		Con_Reportf( S_ERROR "%s: couldn't open %s: %s\n", __func__, filename, strerror( errno ));

	else if( status == WAD_LOAD_BAD_HEADER )
		Con_Reportf( S_ERROR "%s: %s is not a valid WAD2 or WAD3 file\n", __func__, filename );
	else if( status == WAD_LOAD_BAD_FOLDERS )
		Con_Reportf( S_ERROR "%s: %s can't find lump allocation table\n", __func__, filename );
	else if( status == WAD_LOAD_TOO_MANY_FILES )
		Con_Reportf( S_WARN "%s: %s is full (%i lumps)\n", __func__, filename, result.table.lumpCount );
	else if( status == WAD_LOAD_NO_FILES )
		Con_Reportf( S_ERROR "%s: %s has no lumps\n", __func__, filename );
	else if( status == WAD_LOAD_CORRUPTED )
		Con_Reportf( S_ERROR "%s: %s has corrupted lump allocation table\n", __func__, filename );

	if( error )
		*error = status;

	if( status != WAD_LOAD_OK && status != WAD_LOAD_TOO_MANY_FILES )
	{
		Mem_Free( wad );
		return NULL;
	}

	wad->handle = result.handle;
	wad->mempool = result.pool;
	wad->filetime = result.fileTime;
	wad->infotableofs = result.table.infotableOffset;
	wad->numlumps = result.table.lumpCount;
	wad->lumps = result.table.lumps;

	// and leave the file open
	return wad;
}

/*
===========
FS_FileTime_WAD

===========
*/
static int FS_FileTime_WAD_Legacy( searchpath_t *search, const char *filename )
{
	(void)filename;

	return search->wad->filetime;
}

/*
===========
FS_PrintInfo_WAD

===========
*/
static void FS_PrintInfo_WAD_Legacy( searchpath_t *search, char *dst, size_t size )
{
	if( search->wad->handle->searchpath )
		Q_snprintf( dst, size, "%s (%i files)" S_CYAN " from %s" S_DEFAULT, search->filename, search->wad->numlumps, search->wad->handle->searchpath->filename );
	else Q_snprintf( dst, size, "%s (%i files)", search->filename, search->wad->numlumps );
}

/*
===========
FS_FindFile_WAD

===========
*/
static int FS_FindFile_WAD_Legacy( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	fs_wad_archive_view_t archive = W_MakeArchiveView( search );

	return FS_WadBackend_FindFileInArchive( &archive, path, fixedname, len );
}

/*
===========
FS_Search_WAD

===========
*/
static void FS_Search_WAD_Legacy( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	fs_wad_archive_view_t archive = W_MakeArchiveView( search );
	fs_wad_search_runtime_t runtime;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.matchPattern = W_SearchMatchPattern;
	runtime.stringCount = W_SearchStringCount;
	runtime.stringAt = W_SearchStringAt;
	runtime.append = W_SearchAppend;

	FS_WadBackend_SearchArchive( &archive, &runtime, list, pattern, caseinsensitive );
}


/*
===========
W_ReadLump

reading lump into temp buffer
===========
*/
static byte *W_ReadLump_Legacy( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *lumpsizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	fs_wad_archive_view_t archive = W_MakeArchiveView( search );
	fs_wad_read_runtime_t runtime;

	(void)path;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.tell = W_ReadTell;
	runtime.seek = W_ReadSeek;
	runtime.read = W_ReadRead;
	runtime.corrupted = W_ReadCorrupted;
	runtime.allocationFailed = W_ReadAllocationFailed;
	runtime.shortRead = W_ReadShortRead;

	return FS_WadBackend_ReadLump( &archive, &runtime, pack_ind, lumpsizeptr, pfnAlloc, pfnFree );
}

static void FS_Close_WAD_Hook( void *context )
{
	FS_Close_WAD_Legacy( (searchpath_t *)context );
}

static void FS_PrintInfo_WAD_Hook( void *context, char *dst, size_t size )
{
	FS_PrintInfo_WAD_Legacy( (searchpath_t *)context, dst, size );
}

static file_t *FS_OpenFile_WAD_Hook( void *context, const char *filename, const char *mode, int pack_ind )
{
	return FS_OpenFile_WAD_Legacy( (searchpath_t *)context, filename, mode, pack_ind );
}

static int FS_FileTime_WAD_Hook( void *context, const char *filename )
{
	return FS_FileTime_WAD_Legacy( (searchpath_t *)context, filename );
}

static int FS_FindFile_WAD_Hook( void *context, const char *path, char *fixedname, size_t len )
{
	return FS_FindFile_WAD_Legacy( (searchpath_t *)context, path, fixedname, len );
}

static void FS_Search_WAD_Hook( void *context, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	FS_Search_WAD_Legacy( (searchpath_t *)context, list, pattern, caseinsensitive );
}

static byte *W_ReadLump_Hook( void *context, const char *path, int pack_ind, fs_offset_t *lumpsizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	return W_ReadLump_Legacy( (searchpath_t *)context, path, pack_ind, lumpsizeptr, pfnAlloc, pfnFree );
}

static void FS_Close_WAD( searchpath_t *search )
{
	if( search->wad && search->wad->backend )
	{
		void *backend = search->wad->backend;
		search->wad->backend = NULL;
		FS_WadBackendBridge_Close( backend );
		FS_DestroyWadBackendBridge( backend );
		return;
	}

	FS_Close_WAD_Legacy( search );
}

static void FS_PrintInfo_WAD( searchpath_t *search, char *dst, size_t size )
{
	if( search->wad && search->wad->backend )
	{
		FS_WadBackendBridge_PrintInfo( search->wad->backend, dst, size );
		return;
	}

	FS_PrintInfo_WAD_Legacy( search, dst, size );
}

static file_t *FS_OpenFile_WAD( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	if( search->wad && search->wad->backend )
		return FS_WadBackendBridge_OpenFile( search->wad->backend, filename, mode, pack_ind );

	return FS_OpenFile_WAD_Legacy( search, filename, mode, pack_ind );
}

static int FS_FileTime_WAD( searchpath_t *search, const char *filename )
{
	if( search->wad && search->wad->backend )
		return FS_WadBackendBridge_FileTime( search->wad->backend, filename );

	return FS_FileTime_WAD_Legacy( search, filename );
}

static int FS_FindFile_WAD( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	if( search->wad && search->wad->backend )
		return FS_WadBackendBridge_FindFile( search->wad->backend, path, fixedname, len );

	return FS_FindFile_WAD_Legacy( search, path, fixedname, len );
}

static void FS_Search_WAD( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	if( search->wad && search->wad->backend )
	{
		FS_WadBackendBridge_Search( search->wad->backend, list, pattern, caseinsensitive );
		return;
	}

	FS_Search_WAD_Legacy( search, list, pattern, caseinsensitive );
}

static byte *W_ReadLump( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *lumpsizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	if( search->wad && search->wad->backend )
		return FS_WadBackendBridge_LoadFile( search->wad->backend, path, pack_ind, lumpsizeptr, pfnAlloc, pfnFree );

	return W_ReadLump_Legacy( search, path, pack_ind, lumpsizeptr, pfnAlloc, pfnFree );
}

/*
====================
FS_AddWad_Fullpath
====================
*/
searchpath_t *FS_AddWad_Fullpath( const char *wadfile, int flags )
{
	searchpath_t *search;
	wfile_t *wad;
	int errorcode = WAD_LOAD_COULDNT_OPEN;

	wad = W_Open( wadfile, &errorcode, flags );

	if( !wad )
	{
		if( errorcode != WAD_LOAD_NO_FILES )
			Con_Reportf( S_ERROR "%s: unable to load wad \"%s\"\n", __func__, wadfile );
		return NULL;
	}

	search = (searchpath_t *)Mem_Calloc( fs_mempool, sizeof( searchpath_t ));
	Q_strncpy( search->filename, wadfile, sizeof( search->filename ));
	search->wad = wad;
	search->type = SEARCHPATH_WAD;
	search->flags = flags;

	search->pfnPrintInfo = FS_PrintInfo_WAD;
	search->pfnClose = FS_Close_WAD;
	search->pfnOpenFile = FS_OpenFile_WAD;
	search->pfnFileTime = FS_FileTime_WAD;
	search->pfnFindFile = FS_FindFile_WAD;
	search->pfnSearch = FS_Search_WAD;
	search->pfnLoadFile = W_ReadLump;

	{
		fs_wad_backend_hooks_t hooks;
		hooks.context = search;
		hooks.close = FS_Close_WAD_Hook;
		hooks.printInfo = FS_PrintInfo_WAD_Hook;
		hooks.openFile = FS_OpenFile_WAD_Hook;
		hooks.fileTime = FS_FileTime_WAD_Hook;
		hooks.findFile = FS_FindFile_WAD_Hook;
		hooks.search = FS_Search_WAD_Hook;
		hooks.loadFile = W_ReadLump_Hook;
		search->wad->backend = FS_CreateWadBackendBridge( search, &hooks );
	}

	Con_Reportf( "Adding WAD: %s (%i files)\n", wadfile, wad->numlumps );
	return search;
}
