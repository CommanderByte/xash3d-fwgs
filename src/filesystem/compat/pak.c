/*
pak.c - PAK support for filesystem
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

#include "build.h"
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
#include "filesystem/compat/pak_backend_adapter.h"
#include "filesystem/compat/searchpath_mount_adapter.h"
#include "crtlib.h"
#include "common/com_strings.h"

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// PAK errors
#define PAK_LOAD_OK			0
#define PAK_LOAD_COULDNT_OPEN		1
#define PAK_LOAD_BAD_HEADER		2
#define PAK_LOAD_BAD_FOLDERS		3
#define PAK_LOAD_TOO_MANY_FILES	4
#define PAK_LOAD_NO_FILES		5
#define PAK_LOAD_CORRUPTED		6

struct pack_s
{
	file_t *handle;
	int		numfiles;
	void *backend;
	fs_pak_file_entry_t files[]; // flexible
};

static file_t *PAK_OpenSystemFile( void *context, const char *filename, const char *mode )
{
	(void)context;

	return FS_SysOpen( filename, mode );
}

static int PAK_CloseFile( void *context, file_t *file )
{
	(void)context;

	return FS_Close( file );
}

static fs_offset_t PAK_ReadFile( void *context, file_t *file, void *buffer, size_t size )
{
	(void)context;

	return FS_Read( file, buffer, size );
}

static int PAK_SeekFile( void *context, file_t *file, fs_offset_t offset, int whence )
{
	(void)context;

	return FS_Seek( file, offset, whence );
}

static void *PAK_Alloc( void *context, size_t size, int clear )
{
	(void)context;

	if( clear )
		return Mem_Calloc( fs_mempool, size );

	return Mem_Malloc( fs_mempool, size );
}

static void PAK_Free( void *context, void *memory )
{
	(void)context;

	Mem_Free( memory );
}

static fs_pak_archive_view_t PAK_MakeArchiveView( searchpath_t *search )
{
	fs_pak_archive_view_t archive;

	archive.source = search->filename;
	archive.handle = search->pack ? search->pack->handle : NULL;
	archive.fileCount = search->pack ? search->pack->numfiles : 0;
	archive.files = search->pack ? search->pack->files : NULL;

	return archive;
}

static int PAK_SearchMatchPattern( void *context, const char *text, const char *pattern, int caseinsensitive )
{
	(void)context;

	return matchpattern( text, pattern, caseinsensitive );
}

static int PAK_SearchStringCount( void *context, stringlist_t *list )
{
	(void)context;

	return list ? list->numstrings : 0;
}

static const char *PAK_SearchStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;

	if( !list || index < 0 || index >= list->numstrings )
		return NULL;

	return list->strings[index];
}

static void PAK_SearchAppend( void *context, stringlist_t *list, const char *text )
{
	(void)context;

	stringlistappend( list, text );
}

static file_t *PAK_OpenHandle( void *context, file_t *package, int offset, int length )
{
	searchpath_t *search = (searchpath_t *)context;

	return FS_OpenHandle( search, package->handle, offset, length );
}

/*
=================
FS_LoadPackPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t *FS_LoadPackPAK( const char *packfile, int *error )
{
	fs_pak_open_runtime_t runtime;
	fs_pak_open_result_t result;
	pack_t *pack;
	int status;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.openSystem = PAK_OpenSystemFile;
	runtime.close = PAK_CloseFile;
	runtime.read = PAK_ReadFile;
	runtime.seek = PAK_SeekFile;
	runtime.alloc = PAK_Alloc;
	runtime.free = PAK_Free;

	memset( &result, 0, sizeof( result ));
	status = FS_PakBackend_OpenArchive( &runtime, packfile, &result );

	if( status == PAK_LOAD_COULDNT_OPEN )
		Con_Reportf( "%s couldn't open: %s\n", packfile, strerror( errno ));
	else if( status == PAK_LOAD_BAD_HEADER )
		Con_Reportf( "%s is not a packfile. Ignored.\n", packfile );
	else if( status == PAK_LOAD_BAD_FOLDERS )
		Con_Reportf( S_ERROR "%s has an invalid directory size. Ignored.\n", packfile );
	else if( status == PAK_LOAD_TOO_MANY_FILES )
		Con_Reportf( S_ERROR "%s has too many files. Ignored.\n", packfile );
	else if( status == PAK_LOAD_NO_FILES )
		Con_Reportf( "%s has no files. Ignored.\n", packfile );
	else if( status == PAK_LOAD_CORRUPTED )
		Con_Reportf( "%s is an incomplete PAK, not loading\n", packfile );

	if( error )
		*error = status;

	if( status != PAK_LOAD_OK )
		return NULL;

	pack = (pack_t *)Mem_Calloc( fs_mempool, sizeof( pack_t ) + sizeof( fs_pak_file_entry_t ) * result.fileCount );
	if( !pack )
	{
		FS_Close( result.handle );
		Mem_Free( result.files );
		return NULL;
	}

	pack->handle = result.handle;
	pack->numfiles = result.fileCount;
	memcpy( pack->files, result.files, sizeof( fs_pak_file_entry_t ) * result.fileCount );
	Mem_Free( result.files );

#ifdef XASH_REDUCE_FD
	// will reopen when needed
	close( pack->handle );
	pack->handle = -1;
#endif

	return pack;
}

/*
===========
FS_OpenPackedFile

Open a packed file using its package file descriptor
===========
*/
static file_t *FS_OpenFile_PAK_Legacy( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	fs_pak_archive_view_t archive = PAK_MakeArchiveView( search );
	fs_pak_open_file_runtime_t runtime;

	(void)filename;
	(void)mode;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.context = search;
	runtime.openHandle = PAK_OpenHandle;

	return FS_PakBackend_OpenEntry( &archive, &runtime, pack_ind );
}

/*
===========
FS_FindFile_PAK

===========
*/
static int FS_FindFile_PAK_Legacy( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	fs_pak_archive_view_t archive = PAK_MakeArchiveView( search );

	return FS_PakBackend_FindFileInArchive( &archive, path, fixedname, len );
}

/*
===========
FS_Search_PAK

===========
*/
static void FS_Search_PAK_Legacy( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	fs_pak_archive_view_t archive = PAK_MakeArchiveView( search );
	fs_pak_search_runtime_t runtime;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.matchPattern = PAK_SearchMatchPattern;
	runtime.stringCount = PAK_SearchStringCount;
	runtime.stringAt = PAK_SearchStringAt;
	runtime.append = PAK_SearchAppend;

	FS_PakBackend_SearchArchive( &archive, &runtime, list, pattern, caseinsensitive );
}

/*
===========
FS_FileTime_PAK

===========
*/
static int FS_FileTime_PAK_Legacy( searchpath_t *search, const char *filename )
{
	return search->pack->handle->filetime;
}

/*
===========
FS_PrintInfo_PAK

===========
*/
static void FS_PrintInfo_PAK_Legacy( searchpath_t *search, char *dst, size_t size )
{
	if( search->pack->handle->searchpath )
		Q_snprintf( dst, size, "%s (%i files)" S_CYAN " from %s" S_DEFAULT, search->filename, search->pack->numfiles, search->pack->handle->searchpath->filename );
	else Q_snprintf( dst, size, "%s (%i files)", search->filename, search->pack->numfiles );
}

/*
===========
FS_Close_PAK

===========
*/
static void FS_Close_PAK_Legacy( searchpath_t *search )
{
	if( search->pack->handle != NULL )
		FS_Close( search->pack->handle );
	Mem_Free( search->pack );
}

static void FS_Close_PAK_Hook( void *context )
{
	FS_Close_PAK_Legacy( (searchpath_t *)context );
}

static void FS_PrintInfo_PAK_Hook( void *context, char *dst, size_t size )
{
	FS_PrintInfo_PAK_Legacy( (searchpath_t *)context, dst, size );
}

static file_t *FS_OpenFile_PAK_Hook( void *context, const char *filename, const char *mode, int pack_ind )
{
	return FS_OpenFile_PAK_Legacy( (searchpath_t *)context, filename, mode, pack_ind );
}

static int FS_FileTime_PAK_Hook( void *context, const char *filename )
{
	return FS_FileTime_PAK_Legacy( (searchpath_t *)context, filename );
}

static int FS_FindFile_PAK_Hook( void *context, const char *path, char *fixedname, size_t len )
{
	return FS_FindFile_PAK_Legacy( (searchpath_t *)context, path, fixedname, len );
}

static void FS_Search_PAK_Hook( void *context, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	FS_Search_PAK_Legacy( (searchpath_t *)context, list, pattern, caseinsensitive );
}

static void FS_Close_PAK( searchpath_t *search )
{
	if( search->pack && search->pack->backend )
	{
		void *backend = search->pack->backend;
		search->pack->backend = NULL;
		FS_PakBackendBridge_Close( backend );
		FS_DestroyPakBackendBridge( backend );
		return;
	}

	FS_Close_PAK_Legacy( search );
}

static void FS_PrintInfo_PAK( searchpath_t *search, char *dst, size_t size )
{
	if( search->pack && search->pack->backend )
	{
		FS_PakBackendBridge_PrintInfo( search->pack->backend, dst, size );
		return;
	}

	FS_PrintInfo_PAK_Legacy( search, dst, size );
}

static file_t *FS_OpenFile_PAK( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	if( search->pack && search->pack->backend )
		return FS_PakBackendBridge_OpenFile( search->pack->backend, filename, mode, pack_ind );

	return FS_OpenFile_PAK_Legacy( search, filename, mode, pack_ind );
}

static int FS_FileTime_PAK( searchpath_t *search, const char *filename )
{
	if( search->pack && search->pack->backend )
		return FS_PakBackendBridge_FileTime( search->pack->backend, filename );

	return FS_FileTime_PAK_Legacy( search, filename );
}

static int FS_FindFile_PAK( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	if( search->pack && search->pack->backend )
		return FS_PakBackendBridge_FindFile( search->pack->backend, path, fixedname, len );

	return FS_FindFile_PAK_Legacy( search, path, fixedname, len );
}

static void FS_Search_PAK( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	if( search->pack && search->pack->backend )
	{
		FS_PakBackendBridge_Search( search->pack->backend, list, pattern, caseinsensitive );
		return;
	}

	FS_Search_PAK_Legacy( search, list, pattern, caseinsensitive );
}


/*
================
FS_AddPak_Fullpath

Adds the given pack to the search path.
The pack type is autodetected by the file extension.

Returns true if the file was successfully added to the
search path or if it was already included.

If keep_plain_dirs is set, the pack will be added AFTER the first sequence of
plain directories.
================
*/
searchpath_t *FS_AddPak_Fullpath( const char *pakfile, int flags )
{
	searchpath_t *search;
	pack_t *pak;
	int errorcode = PAK_LOAD_COULDNT_OPEN;

	pak = FS_LoadPackPAK( pakfile, &errorcode );

	if( !pak )
	{
		if( errorcode != PAK_LOAD_NO_FILES )
			Con_Reportf( S_ERROR "%s: unable to load pak \"%s\"\n", __func__, pakfile );
		return NULL;
	}

	{
		fs_searchpath_callbacks_t callbacks = {
			FS_PrintInfo_PAK,
			FS_Close_PAK,
			FS_OpenFile_PAK,
			FS_FileTime_PAK,
			FS_FindFile_PAK,
			FS_Search_PAK,
			NULL
		};
		search = FS_SearchPath_Alloc();
		if( !search )
		{
			if( pak->handle )
				FS_Close( pak->handle );
			Mem_Free( pak );
			return NULL;
		}
		FS_SearchPath_Init( search, pakfile, SEARCHPATH_PAK, flags, &callbacks );
	}
	search->pack = pak;

	{
		fs_pak_backend_hooks_t hooks;
		hooks.context = search;
		hooks.close = FS_Close_PAK_Hook;
		hooks.printInfo = FS_PrintInfo_PAK_Hook;
		hooks.openFile = FS_OpenFile_PAK_Hook;
		hooks.fileTime = FS_FileTime_PAK_Hook;
		hooks.findFile = FS_FindFile_PAK_Hook;
		hooks.search = FS_Search_PAK_Hook;
		search->pack->backend = FS_CreatePakBackendBridge( search, &hooks );
	}

	Con_Reportf( "Adding PAK: %s (%i files)\n", pakfile, pak->numfiles );

	return search;
}

/*
================
FS_CheckForQuakePak

To generate fake gameinfo for Quake directory, we need to parse pak0.pak
and find progs.dat in it
================
*/
qboolean FS_CheckForQuakePak( const char *pakfile, const char *files[], size_t num_files )
{
	qboolean is_quake = false;
	pack_t *pak;
	int i;

	pak = FS_LoadPackPAK( pakfile, NULL );
	if( !pak )
		return false;

	for( i = 0; i < num_files; i++ )
	{
		int j;

		for( j = 0; j < pak->numfiles; j++ )
		{
			if( Q_strchr( pak->files[j].name, '/' ))
				continue; // exclude subdirectories

			if( !Q_stricmp( pak->files[j].name, files[i] ))
			{
				is_quake = true;
				break;
			}
		}

		if( is_quake )
			break;
	}

	Mem_Free( pak );
	return is_quake;
}
