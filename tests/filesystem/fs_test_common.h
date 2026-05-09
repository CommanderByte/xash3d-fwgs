#ifndef FS_TEST_COMMON_H
#define FS_TEST_COMMON_H

#include "port.h"
#include "build.h"
#include "filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if XASH_WIN32
#include <direct.h>
#define FS_TEST_MKDIR( x ) _mkdir( x )
#define FS_TEST_RMDIR( x ) _rmdir( x )
#else
#include <sys/stat.h>
#include <unistd.h>
#define FS_TEST_MKDIR( x ) mkdir( x, 0777 )
#define FS_TEST_RMDIR( x ) rmdir( x )
#endif

#if XASH_POSIX
#include <dlfcn.h>
typedef void *fs_test_module_t;
#define FS_TEST_LOAD_LIBRARY( x ) dlopen( x, RTLD_NOW )
#define FS_TEST_GET_PROC_ADDRESS( x, y ) dlsym( x, y )
#define FS_TEST_FREE_LIBRARY( x ) dlclose( x )
#elif XASH_WIN32
#include <windows.h>
typedef HMODULE fs_test_module_t;
#define FS_TEST_LOAD_LIBRARY( x ) LoadLibrary( x )
#define FS_TEST_GET_PROC_ADDRESS( x, y ) GetProcAddress( x, y )
#define FS_TEST_FREE_LIBRARY( x ) FreeLibrary( x )
#endif

static qboolean FS_TestLoadFilesystem( fs_test_module_t *module, fs_api_t *fs, fs_globals_t **globals )
{
	FSAPI get_fs_api;

	*module = FS_TEST_LOAD_LIBRARY( "filesystem_stdio." OS_LIB_EXT );
	if( !*module )
	{
		printf( "failed to load filesystem_stdio.%s\n", OS_LIB_EXT );
		return false;
	}

	get_fs_api = (FSAPI)FS_TEST_GET_PROC_ADDRESS( *module, GET_FS_API );
	if( !get_fs_api )
	{
		printf( "failed to find %s\n", GET_FS_API );
		return false;
	}

	if( !get_fs_api( FS_API_VERSION, fs, globals, NULL ))
	{
		printf( "failed to initialize filesystem API\n" );
		return false;
	}

	return true;
}

static void FS_TestSeedRand( void )
{
	srand( (unsigned)time( NULL ));
}

static void FS_TestMakeUniqueName( char *buffer, size_t size, const char *prefix )
{
	snprintf( buffer, size, "%s_%u_%u", prefix, (unsigned)time( NULL ), (unsigned)rand() );
}

static qboolean FS_TestCreateDirectory( const char *path )
{
	if( FS_TEST_MKDIR( path ) == 0 )
		return true;

	return false;
}

static qboolean FS_TestWriteFile( const char *path, const void *data, size_t size )
{
	FILE *file = fopen( path, "wb" );
	if( !file )
		return false;

	if( fwrite( data, 1, size, file ) != size )
	{
		fclose( file );
		return false;
	}

	fclose( file );
	return true;
}

static void FS_TestRemoveFile( const char *path )
{
	remove( path );
}

static void FS_TestRemoveDirectory( const char *path )
{
	FS_TEST_RMDIR( path );
}

#endif // FS_TEST_COMMON_H
