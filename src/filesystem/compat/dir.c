/*
dir.c - caseinsensitive directory operations
Copyright (C) 2022 Alibek Omarov, Velaron
Copyright (C) 2023 Xash3D FWGS contributors

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
#include <errno.h>
#include <stddef.h>
#if XASH_POSIX
#include <unistd.h>
#if !XASH_PSVITA
#include <sys/ioctl.h>
#endif
#endif
#if XASH_LINUX
#include <linux/fs.h>
#ifndef FS_CASEFOLD_FL // for compatibility with older distros
#define FS_CASEFOLD_FL 0x40000000
#endif // FS_CASEFOLD_FL
#endif // XASH_LINUX

#include "port.h"
#include "filesystem_internal.h"
#include "filesystem/compat/dir_backend_adapter.h"
#include "filesystem/compat/searchpath_mount_adapter.h"
#include "crtlib.h"
#include "xash3d_mathlib.h"
#include "common/com_strings.h"

typedef struct dir_s
{
	string name;
	int numentries;
	struct dir_s *entries; // sorted
	void *backend;
} dir_t;

static qboolean Platform_GetDirectoryCaseSensitivity( const char *dir )
{
#if XASH_WIN32 || XASH_PSVITA || XASH_NSWITCH
	return false;
#elif XASH_ANDROID
	// on Android, doing code below causes crash in MediaProviderGoogle.apk!libfuse_jni.so
	// which in turn makes vold (Android's Volume Daemon) to umount /storage/emulated/0
	// and because you can't unmount a filesystem when there is file descriptors open
	// it has no other choice but to terminate and then kill our program
	return true;
#elif XASH_LINUX && defined( FS_IOC_GETFLAGS )
	int flags = 0;
	int fd;

	fd = open( dir, O_RDONLY | O_NONBLOCK );
	if( fd < 0 )
		return true;

	if( ioctl( fd, FS_IOC_GETFLAGS, &flags ) < 0 )
	{
		close( fd );
		return true;
	}

	close( fd );

	return !FBitSet( flags, FS_CASEFOLD_FL );
#else
	return true;
#endif
}

static void *FS_DirAlloc( void *context, size_t size, int clear )
{
	(void)context;
	return clear ? Mem_Calloc( fs_mempool, size ) : Mem_Malloc( fs_mempool, size );
}

static void FS_DirFree( void *context, void *memory )
{
	(void)context;
	Mem_Free( memory );
}

static int FS_DirFolderExists( void *context, const char *path )
{
	(void)context;
	return FS_SysFolderExists( path );
}

static int FS_DirFileExists( void *context, const char *path )
{
	(void)context;
	return FS_SysFileExists( path );
}

static int FS_DirFileOrFolderExists( void *context, const char *path )
{
	(void)context;
	return FS_SysFileOrFolderExists( path );
}

static int FS_DirIsDirectoryCaseSensitive( void *context, const char *path )
{
	(void)context;
	return Platform_GetDirectoryCaseSensitivity( path );
}

static stringlist_t *FS_DirListCreate( void *context )
{
	stringlist_t *list;

	(void)context;
	list = Mem_Calloc( fs_mempool, sizeof( *list ));
	stringlistinit( list );
	return list;
}

static void FS_DirListDirectory( void *context, stringlist_t *list, const char *path, int dirsOnly )
{
	(void)context;
	listdirectory( list, path, dirsOnly );
}

static void FS_DirListDestroy( void *context, stringlist_t *list )
{
	(void)context;
	stringlistfreecontents( list );
	Mem_Free( list );
}

static int FS_DirStringCount( void *context, stringlist_t *list )
{
	(void)context;
	return list ? list->numstrings : 0;
}

static const char *FS_DirStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;
	return ( list && index >= 0 && index < list->numstrings ) ? list->strings[index] : NULL;
}

static void FS_DirOverflow( void *context, const char *path, const char *operation )
{
	(void)context;
	Con_Printf( S_ERROR "FS_FixFileCase: overflow while appending %s (%s)\n", path, operation );
}

static fs_directory_case_runtime_t FS_MakeDirectoryCaseRuntime( void )
{
	fs_directory_case_runtime_t runtime;

	runtime.context = NULL;
	runtime.alloc = FS_DirAlloc;
	runtime.free = FS_DirFree;
	runtime.folderExists = FS_DirFolderExists;
	runtime.fileExists = FS_DirFileExists;
	runtime.fileOrFolderExists = FS_DirFileOrFolderExists;
	runtime.isDirectoryCaseSensitive = FS_DirIsDirectoryCaseSensitive;
	runtime.listCreate = FS_DirListCreate;
	runtime.listDirectory = FS_DirListDirectory;
	runtime.listDestroy = FS_DirListDestroy;
	runtime.stringCount = FS_DirStringCount;
	runtime.stringAt = FS_DirStringAt;
	runtime.overflow = FS_DirOverflow;

	return runtime;
}

qboolean FS_FixFileCase( dir_t *dir, const char *path, char *dst, const size_t len, qboolean createpath )
{
	fs_directory_case_runtime_t runtime = FS_MakeDirectoryCaseRuntime();
	return FS_DirectoryBackend_FixFileCase( dir, &runtime, path, dst, len, createpath );
}

static void FS_Close_DIR_Legacy( searchpath_t *search )
{
	fs_directory_case_runtime_t runtime = FS_MakeDirectoryCaseRuntime();
	FS_DirectoryBackend_FreeEntries( search->dir, &runtime );
	Mem_Free( search->dir );
}

static void FS_PrintInfo_DIR_Legacy( searchpath_t *search, char *dst, size_t size )
{
	Q_strncpy( dst, search->filename, size );
}

static int FS_FindFile_DIR_Legacy( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	fs_directory_case_runtime_t runtime = FS_MakeDirectoryCaseRuntime();

	return FS_DirectoryBackend_FindFileInDirectory( search->dir, &runtime,
		search->filename, path, fixedname, len );
}

static int FS_DirSearchMatchPattern( void *context, const char *text, const char *pattern, int caseInsensitive )
{
	(void)context;
	return matchpattern( text, pattern, caseInsensitive );
}

static int FS_DirSearchStringCount( void *context, stringlist_t *list )
{
	(void)context;
	return list ? list->numstrings : 0;
}

static const char *FS_DirSearchStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;
	return ( list && index >= 0 && index < list->numstrings ) ? list->strings[index] : NULL;
}

static void FS_DirSearchAppend( void *context, stringlist_t *list, const char *text )
{
	(void)context;
	stringlistappend( list, text );
}

static fs_directory_search_runtime_t FS_MakeDirectorySearchRuntime( void )
{
	fs_directory_search_runtime_t runtime;

	runtime.context = NULL;
	runtime.caseRuntime = FS_MakeDirectoryCaseRuntime();
	runtime.matchPattern = FS_DirSearchMatchPattern;
	runtime.stringCount = FS_DirSearchStringCount;
	runtime.stringAt = FS_DirSearchStringAt;
	runtime.append = FS_DirSearchAppend;

	return runtime;
}

static void FS_Search_DIR_Legacy( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	fs_directory_search_runtime_t runtime = FS_MakeDirectorySearchRuntime();
	FS_DirectoryBackend_SearchDirectory( search->dir, &runtime, list, pattern, caseinsensitive );
}

static int FS_FileTime_DIR_Legacy( searchpath_t *search, const char *filename )
{
	char path[MAX_SYSPATH];

	Q_snprintf( path, sizeof( path ), "%s%s", search->filename, filename );
	return FS_SysFileTime( path );
}

static file_t *FS_DirOpenSystem( void *context, const char *path, const char *mode );
static void FS_DirSetSearchPath( void *context, file_t *file, void *searchPath );
static fs_directory_open_runtime_t FS_MakeDirectoryOpenRuntime( void );

static file_t *FS_OpenFile_DIR_Legacy( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	fs_directory_open_runtime_t runtime = FS_MakeDirectoryOpenRuntime();

	(void)pack_ind;

	return FS_DirectoryBackend_OpenFile( search->dir, &runtime, search,
		search->filename, filename, mode );
}

static file_t *FS_DirOpenSystem( void *context, const char *path, const char *mode )
{
	(void)context;
	return FS_SysOpen( path, mode );
}

static void FS_DirSetSearchPath( void *context, file_t *file, void *searchPath )
{
	(void)context;
	if( file )
		file->searchpath = (searchpath_t *)searchPath;
}

static fs_directory_open_runtime_t FS_MakeDirectoryOpenRuntime( void )
{
	fs_directory_open_runtime_t runtime;

	runtime.context = NULL;
	runtime.caseRuntime = FS_MakeDirectoryCaseRuntime();
	runtime.openSystem = FS_DirOpenSystem;
	runtime.setSearchPath = FS_DirSetSearchPath;

	return runtime;
}

static void FS_Close_DIR_Hook( void *context )
{
	FS_Close_DIR_Legacy( (searchpath_t *)context );
}

static void FS_PrintInfo_DIR_Hook( void *context, char *dst, size_t size )
{
	FS_PrintInfo_DIR_Legacy( (searchpath_t *)context, dst, size );
}

static file_t *FS_OpenFile_DIR_Hook( void *context, const char *filename, const char *mode, int pack_ind )
{
	return FS_OpenFile_DIR_Legacy( (searchpath_t *)context, filename, mode, pack_ind );
}

static int FS_FileTime_DIR_Hook( void *context, const char *filename )
{
	return FS_FileTime_DIR_Legacy( (searchpath_t *)context, filename );
}

static int FS_FindFile_DIR_Hook( void *context, const char *path, char *fixedname, size_t len )
{
	return FS_FindFile_DIR_Legacy( (searchpath_t *)context, path, fixedname, len );
}

static void FS_Search_DIR_Hook( void *context, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	FS_Search_DIR_Legacy( (searchpath_t *)context, list, pattern, caseinsensitive );
}

static void FS_Close_DIR( searchpath_t *search )
{
	if( search->dir && search->dir->backend )
	{
		void *backend = search->dir->backend;
		FS_DirectoryBackendBridge_Close( backend );
		FS_DestroyDirectoryBackendBridge( backend );
		return;
	}

	FS_Close_DIR_Legacy( search );
}

static void FS_PrintInfo_DIR( searchpath_t *search, char *dst, size_t size )
{
	if( search->dir && search->dir->backend )
	{
		FS_DirectoryBackendBridge_PrintInfo( search->dir->backend, dst, size );
		return;
	}

	FS_PrintInfo_DIR_Legacy( search, dst, size );
}

static int FS_FindFile_DIR( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	if( search->dir && search->dir->backend )
		return FS_DirectoryBackendBridge_FindFile( search->dir->backend, path, fixedname, len );

	return FS_FindFile_DIR_Legacy( search, path, fixedname, len );
}

static void FS_Search_DIR( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	if( search->dir && search->dir->backend )
	{
		FS_DirectoryBackendBridge_Search( search->dir->backend, list, pattern, caseinsensitive );
		return;
	}

	FS_Search_DIR_Legacy( search, list, pattern, caseinsensitive );
}

static int FS_FileTime_DIR( searchpath_t *search, const char *filename )
{
	if( search->dir && search->dir->backend )
		return FS_DirectoryBackendBridge_FileTime( search->dir->backend, filename );

	return FS_FileTime_DIR_Legacy( search, filename );
}

static file_t *FS_OpenFile_DIR( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	if( search->dir && search->dir->backend )
		return FS_DirectoryBackendBridge_OpenFile( search->dir->backend, filename, mode, pack_ind );

	return FS_OpenFile_DIR_Legacy( search, filename, mode, pack_ind );
}

void FS_InitDirectorySearchpath( searchpath_t *search, const char *path, int flags )
{
	fs_searchpath_callbacks_t callbacks = {
		FS_PrintInfo_DIR,
		FS_Close_DIR,
		FS_OpenFile_DIR,
		FS_FileTime_DIR,
		FS_FindFile_DIR,
		FS_Search_DIR,
		NULL
	};
	searchpathtype_t type;

	if( !Q_stricmp( COM_FileExtension( path ), "pk3dir" ))
		type = SEARCHPATH_PK3DIR;
	else type = SEARCHPATH_PLAIN;

	FS_SearchPath_Init( search, path, type, flags, &callbacks );
	if( !search )
		return;
	COM_PathSlashFix( search->filename );

	// create cache root
	search->dir = Mem_Malloc( fs_mempool, sizeof( dir_t ));
	Q_strncpy( search->dir->name, search->filename, sizeof( search->dir->name ));
	search->dir->backend = NULL;
	{
		fs_directory_case_runtime_t runtime = FS_MakeDirectoryCaseRuntime();
		FS_DirectoryBackend_PopulateEntries( search->dir, path, &runtime );
	}
	{
		fs_directory_backend_hooks_t hooks;
		hooks.context = search;
		hooks.close = FS_Close_DIR_Hook;
		hooks.printInfo = FS_PrintInfo_DIR_Hook;
		hooks.openFile = FS_OpenFile_DIR_Hook;
		hooks.fileTime = FS_FileTime_DIR_Hook;
		hooks.findFile = FS_FindFile_DIR_Hook;
		hooks.search = FS_Search_DIR_Hook;
		search->dir->backend = FS_CreateDirectoryBackendBridge( search, &hooks );
	}
}

searchpath_t *FS_AddDir_Fullpath( const char *path, int flags )
{
	searchpath_t *search;

	search = FS_SearchPath_Alloc();
	if( !search )
		return NULL;

	FS_InitDirectorySearchpath( search, path, flags );
	Con_Printf( "Adding directory: %s\n", path );

	return search;
}
