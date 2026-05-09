/*
zip.c - ZIP support for filesystem
Copyright (C) 2019 Mr0maks
Copyright (C) 2019-2023 Xash3D FWGS contributors

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
#include <stdint.h>
#include "port.h"
#include "filesystem_internal.h"
#include "filesystem/compat/zip_backend_adapter.h"
#include "filesystem/compat/searchpath_mount_adapter.h"
#include "crtlib.h"
#include "common/com_strings.h"

#define ZIP_COMPRESSION_NO_COMPRESSION	    0
#define ZIP_COMPRESSION_DEFLATED	    8

// ZIP errors
enum
{
	ZIP_LOAD_OK = 0,
	ZIP_LOAD_COULDNT_OPEN,
	ZIP_LOAD_BAD_HEADER,
	ZIP_LOAD_BAD_FOLDERS,
	ZIP_LOAD_NO_FILES,
	ZIP_LOAD_CORRUPTED
};

struct zip_s
{
	file_t *handle;
	int		numfiles;
	void *backend;
	fs_zip_file_entry_t files[]; // flexible
};

// #define ENABLE_CRC_CHECK // known to be buggy because of possible libpublic crc32 bug, disabled

/*
============
FS_CloseZIP
============
*/
static void FS_CloseZIP( zip_t *zip )
{
	if( zip->handle != NULL )
		FS_Close( zip->handle );

	Mem_Free( zip );
}

static file_t *ZIP_OpenSystemFile( void *context, const char *filename, const char *mode )
{
	(void)context;

	return FS_SysOpen( filename, mode );
}

static int ZIP_CloseFile( void *context, file_t *file )
{
	(void)context;

	return FS_Close( file );
}

static fs_offset_t ZIP_ReadFile( void *context, file_t *file, void *buffer, size_t size )
{
	(void)context;

	return FS_Read( file, buffer, size );
}

static int ZIP_SeekFile( void *context, file_t *file, fs_offset_t offset, int whence )
{
	(void)context;

	return FS_Seek( file, offset, whence );
}

static fs_offset_t ZIP_FileLength( void *context, file_t *file )
{
	(void)context;

	return file ? file->real_length : -1;
}

static void *ZIP_Alloc( void *context, size_t size, int clear )
{
	(void)context;

	if( clear )
		return Mem_Calloc( fs_mempool, size );

	return Mem_Malloc( fs_mempool, size );
}

static void ZIP_Free( void *context, void *memory )
{
	(void)context;

	Mem_Free( memory );
}

static fs_zip_archive_view_t ZIP_MakeArchiveView( searchpath_t *search )
{
	fs_zip_archive_view_t archive;

	archive.source = search->filename;
	archive.handle = search->zip ? search->zip->handle : NULL;
	archive.fileCount = search->zip ? search->zip->numfiles : 0;
	archive.files = search->zip ? search->zip->files : NULL;

	return archive;
}

static int ZIP_SearchMatchPattern( void *context, const char *text, const char *pattern, int caseinsensitive )
{
	(void)context;

	return matchpattern( text, pattern, caseinsensitive );
}

static int ZIP_SearchStringCount( void *context, stringlist_t *list )
{
	(void)context;

	return list ? list->numstrings : 0;
}

static const char *ZIP_SearchStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;

	if( !list || index < 0 || index >= list->numstrings )
		return NULL;

	return list->strings[index];
}

static void ZIP_SearchAppend( void *context, stringlist_t *list, const char *text )
{
	(void)context;

	stringlistappend( list, text );
}

static file_t *ZIP_OpenHandle( void *context, file_t *package, fs_offset_t offset, fs_offset_t length )
{
	searchpath_t *search = (searchpath_t *)context;

	return FS_OpenHandle( search, package->handle, offset, length );
}

static int ZIP_SetupDeflated( void *context, file_t *file, fs_offset_t compressed_size, const char *filename )
{
	ztoolkit_t *ztk;

	(void)context;

	SetBits( file->flags, FILE_DEFLATED );

	ztk = Mem_Calloc( fs_mempool, sizeof( *ztk ));
	ztk->comp_length = compressed_size;
	ztk->zstream.next_in = ztk->input;
	ztk->zstream.avail_in = 0;

	if( inflateInit2( &ztk->zstream, -MAX_WBITS ) != Z_OK )
	{
		Con_Printf( "%s: inflate init error (file: %s)\n", __func__, filename );
		Mem_Free( ztk );
		return false;
	}

	ztk->zstream.next_out = file->buff;
	ztk->zstream.avail_out = sizeof( file->buff );
	file->ztk = ztk;
	return true;
}

static void ZIP_UnsupportedCompression( void *context, const char *filename )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: %s: file compressed with unknown algorithm.\n", __func__, filename );
}

static void *ZIP_TempAlloc( void *context, size_t size )
{
	(void)context;

	return Mem_Malloc( fs_mempool, size );
}

static void ZIP_TempFree( void *context, void *memory )
{
	(void)context;

	Mem_Free( memory );
}

static void ZIP_LoadAllocationFailed( void *context, size_t size )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: can't alloc %li bytes, no free memory\n", __func__, (long)size );
}

static void ZIP_LoadSizeMismatch( void *context, const char *filename )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: %s size doesn't match\n", __func__, filename );
}

static void ZIP_LoadInflateFailed( void *context, int code )
{
	(void)context;
	(void)code;

	Con_Printf( S_ERROR "%s: inflateInit2 failed\n", __func__ );
}

static void ZIP_LoadDecompressFailed( void *context, const char *filename, int code )
{
	(void)context;

	Con_Reportf( S_ERROR "%s: %s: error while file decompressing. Zlib return code %d.\n", __func__, filename, code );
}

static int ZIP_LoadInflateRaw( void *context, const void *compressed, size_t compressed_size, void *output, size_t output_size, const char *filename )
{
	z_stream decompress_stream;
	int zlib_result;

	(void)context;

	memset( &decompress_stream, 0, sizeof( decompress_stream ) );
	decompress_stream.total_in = decompress_stream.avail_in = compressed_size;
	decompress_stream.next_in = (Bytef *)compressed;
	decompress_stream.total_out = decompress_stream.avail_out = output_size;
	decompress_stream.next_out = (Bytef *)output;
	decompress_stream.zalloc = Z_NULL;
	decompress_stream.zfree = Z_NULL;
	decompress_stream.opaque = Z_NULL;

	if( inflateInit2( &decompress_stream, -MAX_WBITS ) != Z_OK )
	{
		ZIP_LoadInflateFailed( context, Z_DATA_ERROR );
		return false;
	}

	zlib_result = inflate( &decompress_stream, Z_NO_FLUSH );
	inflateEnd( &decompress_stream );

	if( zlib_result == Z_OK || zlib_result == Z_STREAM_END )
		return true;

	ZIP_LoadDecompressFailed( context, filename, zlib_result );
	return false;
}

/*
============
FS_Close_ZIP
============
*/
static void FS_Close_ZIP_Legacy( searchpath_t *search )
{
	FS_CloseZIP( search->zip );
}

/*
============
FS_LoadZip
============
*/
static zip_t *FS_LoadZip( const char *zipfile, int *error )
{
	fs_zip_open_runtime_t runtime;
	fs_zip_open_result_t result;
	zip_t *zip;
	int status;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.openSystem = ZIP_OpenSystemFile;
	runtime.close = ZIP_CloseFile;
	runtime.read = ZIP_ReadFile;
	runtime.seek = ZIP_SeekFile;
	runtime.length = ZIP_FileLength;
	runtime.alloc = ZIP_Alloc;
	runtime.free = ZIP_Free;

	memset( &result, 0, sizeof( result ));
	status = FS_ZipBackend_OpenArchive( &runtime, zipfile, &result );

	if( status == ZIP_LOAD_COULDNT_OPEN )
		Con_Reportf( S_ERROR "%s couldn't open\n", zipfile );
	else if( status == ZIP_LOAD_BAD_HEADER )
		Con_Reportf( S_ERROR "%s is not a zip file or is corrupted. Ignored.\n", zipfile );
	else if( status == ZIP_LOAD_NO_FILES )
		Con_Reportf( S_WARN "%s has no files. Ignored.\n", zipfile );
	else if( status == ZIP_LOAD_CORRUPTED )
		Con_Reportf( S_ERROR "%s is an incomplete ZIP, not loading\n", zipfile );

	if( error )
		*error = status;

	if( status != ZIP_LOAD_OK )
		return NULL;

	zip = (zip_t *)Mem_Calloc( fs_mempool, sizeof( *zip ) + sizeof( fs_zip_file_entry_t ) * result.fileCount );
	if( !zip )
	{
		FS_Close( result.handle );
		Mem_Free( result.files );
		return NULL;
	}

	zip->handle = result.handle;
	zip->numfiles = result.fileCount;
	memcpy( zip->files, result.files, sizeof( fs_zip_file_entry_t ) * result.fileCount );
	Mem_Free( result.files );

	return zip;
}

/*
===========
FS_OpenZipFile

Open a packed file using its package file descriptor
===========
*/
static file_t *FS_OpenFile_ZIP_Legacy( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	fs_zip_archive_view_t archive = ZIP_MakeArchiveView( search );
	fs_zip_open_file_runtime_t runtime;

	(void)filename;
	(void)mode;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.context = search;
	runtime.openHandle = ZIP_OpenHandle;
	runtime.setupDeflated = ZIP_SetupDeflated;
	runtime.close = ZIP_CloseFile;
	runtime.unsupportedCompression = ZIP_UnsupportedCompression;

	return FS_ZipBackend_OpenEntry( &archive, &runtime, pack_ind );
}

/*
===========
FS_LoadZIPFile

===========
*/
static byte *FS_LoadZIPFile_Legacy( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *sizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	fs_zip_archive_view_t archive = ZIP_MakeArchiveView( search );
	fs_zip_load_file_runtime_t runtime;

	(void)path;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.seek = ZIP_SeekFile;
	runtime.read = ZIP_ReadFile;
	runtime.tempAlloc = ZIP_TempAlloc;
	runtime.tempFree = ZIP_TempFree;
	runtime.allocationFailed = ZIP_LoadAllocationFailed;
	runtime.sizeMismatch = ZIP_LoadSizeMismatch;
	runtime.inflateFailed = ZIP_LoadInflateFailed;
	runtime.decompressFailed = ZIP_LoadDecompressFailed;
	runtime.inflateRaw = ZIP_LoadInflateRaw;
	runtime.unsupportedCompression = ZIP_UnsupportedCompression;

	return FS_ZipBackend_LoadEntry( &archive, &runtime, pack_ind, sizeptr, pfnAlloc, pfnFree );
}

/*
===========
FS_FileTime_ZIP

===========
*/
static int FS_FileTime_ZIP_Legacy( searchpath_t *search, const char *filename )
{
	(void)filename;

	return search->zip->handle->filetime;
}

/*
===========
FS_PrintInfo_ZIP

===========
*/
static void FS_PrintInfo_ZIP_Legacy( searchpath_t *search, char *dst, size_t size )
{
	if( search->zip->handle->searchpath )
		Q_snprintf( dst, size, "%s (%i files)" S_CYAN " from %s" S_DEFAULT, search->filename, search->zip->numfiles, search->zip->handle->searchpath->filename );
	else Q_snprintf( dst, size, "%s (%i files)", search->filename, search->zip->numfiles );
}

/*
===========
FS_FindFile_ZIP

===========
*/
static int FS_FindFile_ZIP_Legacy( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	fs_zip_archive_view_t archive = ZIP_MakeArchiveView( search );

	return FS_ZipBackend_FindFileInArchive( &archive, path, fixedname, len );
}

/*
===========
FS_Search_ZIP

===========
*/
static void FS_Search_ZIP_Legacy( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	fs_zip_archive_view_t archive = ZIP_MakeArchiveView( search );
	fs_zip_search_runtime_t runtime;

	memset( &runtime, 0, sizeof( runtime ));
	runtime.matchPattern = ZIP_SearchMatchPattern;
	runtime.stringCount = ZIP_SearchStringCount;
	runtime.stringAt = ZIP_SearchStringAt;
	runtime.append = ZIP_SearchAppend;

	FS_ZipBackend_SearchArchive( &archive, &runtime, list, pattern, caseinsensitive );
}

static void FS_Close_ZIP_Hook( void *context )
{
	FS_Close_ZIP_Legacy( (searchpath_t *)context );
}

static void FS_PrintInfo_ZIP_Hook( void *context, char *dst, size_t size )
{
	FS_PrintInfo_ZIP_Legacy( (searchpath_t *)context, dst, size );
}

static file_t *FS_OpenFile_ZIP_Hook( void *context, const char *filename, const char *mode, int pack_ind )
{
	return FS_OpenFile_ZIP_Legacy( (searchpath_t *)context, filename, mode, pack_ind );
}

static int FS_FileTime_ZIP_Hook( void *context, const char *filename )
{
	return FS_FileTime_ZIP_Legacy( (searchpath_t *)context, filename );
}

static int FS_FindFile_ZIP_Hook( void *context, const char *path, char *fixedname, size_t len )
{
	return FS_FindFile_ZIP_Legacy( (searchpath_t *)context, path, fixedname, len );
}

static void FS_Search_ZIP_Hook( void *context, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	FS_Search_ZIP_Legacy( (searchpath_t *)context, list, pattern, caseinsensitive );
}

static byte *FS_LoadZIPFile_Hook( void *context, const char *path, int pack_ind, fs_offset_t *sizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	return FS_LoadZIPFile_Legacy( (searchpath_t *)context, path, pack_ind, sizeptr, pfnAlloc, pfnFree );
}

static void FS_Close_ZIP( searchpath_t *search )
{
	if( search->zip && search->zip->backend )
	{
		void *backend = search->zip->backend;
		search->zip->backend = NULL;
		FS_ZipBackendBridge_Close( backend );
		FS_DestroyZipBackendBridge( backend );
		return;
	}

	FS_Close_ZIP_Legacy( search );
}

static void FS_PrintInfo_ZIP( searchpath_t *search, char *dst, size_t size )
{
	if( search->zip && search->zip->backend )
	{
		FS_ZipBackendBridge_PrintInfo( search->zip->backend, dst, size );
		return;
	}

	FS_PrintInfo_ZIP_Legacy( search, dst, size );
}

static file_t *FS_OpenFile_ZIP( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	if( search->zip && search->zip->backend )
		return FS_ZipBackendBridge_OpenFile( search->zip->backend, filename, mode, pack_ind );

	return FS_OpenFile_ZIP_Legacy( search, filename, mode, pack_ind );
}

static int FS_FileTime_ZIP( searchpath_t *search, const char *filename )
{
	if( search->zip && search->zip->backend )
		return FS_ZipBackendBridge_FileTime( search->zip->backend, filename );

	return FS_FileTime_ZIP_Legacy( search, filename );
}

static int FS_FindFile_ZIP( searchpath_t *search, const char *path, char *fixedname, size_t len )
{
	if( search->zip && search->zip->backend )
		return FS_ZipBackendBridge_FindFile( search->zip->backend, path, fixedname, len );

	return FS_FindFile_ZIP_Legacy( search, path, fixedname, len );
}

static void FS_Search_ZIP( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	if( search->zip && search->zip->backend )
	{
		FS_ZipBackendBridge_Search( search->zip->backend, list, pattern, caseinsensitive );
		return;
	}

	FS_Search_ZIP_Legacy( search, list, pattern, caseinsensitive );
}

static byte *FS_LoadZIPFile( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *sizeptr, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	if( search->zip && search->zip->backend )
		return FS_ZipBackendBridge_LoadFile( search->zip->backend, path, pack_ind, sizeptr, pfnAlloc, pfnFree );

	return FS_LoadZIPFile_Legacy( search, path, pack_ind, sizeptr, pfnAlloc, pfnFree );
}

/*
===========
FS_AddZip_Fullpath

===========
*/
searchpath_t *FS_AddZip_Fullpath( const char *zipfile, int flags )
{
	searchpath_t *search;
	zip_t *zip;
	int errorcode = ZIP_LOAD_COULDNT_OPEN;

	zip = FS_LoadZip( zipfile, &errorcode );

	if( !zip )
	{
		if( errorcode != ZIP_LOAD_NO_FILES )
			Con_Reportf( S_ERROR "%s: unable to load zip \"%s\"\n", __func__, zipfile );
		return NULL;
	}

	{
		fs_searchpath_callbacks_t callbacks = {
			FS_PrintInfo_ZIP,
			FS_Close_ZIP,
			FS_OpenFile_ZIP,
			FS_FileTime_ZIP,
			FS_FindFile_ZIP,
			FS_Search_ZIP,
			FS_LoadZIPFile
		};
		search = FS_SearchPath_Alloc();
		if( !search )
		{
			FS_CloseZIP( zip );
			return NULL;
		}
		FS_SearchPath_Init( search, zipfile, SEARCHPATH_ZIP, flags, &callbacks );
	}
	search->zip = zip;

	{
		fs_zip_backend_hooks_t hooks;
		hooks.context = search;
		hooks.close = FS_Close_ZIP_Hook;
		hooks.printInfo = FS_PrintInfo_ZIP_Hook;
		hooks.openFile = FS_OpenFile_ZIP_Hook;
		hooks.fileTime = FS_FileTime_ZIP_Hook;
		hooks.findFile = FS_FindFile_ZIP_Hook;
		hooks.search = FS_Search_ZIP_Hook;
		hooks.loadFile = FS_LoadZIPFile_Hook;
		search->zip->backend = FS_CreateZipBackendBridge( search, &hooks );
	}

	Con_Reportf( "Adding ZIP: %s (%i files)\n", zipfile, zip->numfiles );
	return search;
}
