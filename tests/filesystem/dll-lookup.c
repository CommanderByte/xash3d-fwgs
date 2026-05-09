#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"

fs_test_module_t g_hModule;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

static qboolean LoadFilesystem( void )
{
	return FS_TestLoadFilesystem( &g_hModule, &g_fs, &g_nullglobals );
}

static qboolean WriteGameInfoFixture( void )
{
	static const char gameinfo[] =
		"title \"Base Game\"\n"
		"basedir \"valve\"\n"
		"gamedll \"dlls/hl.dll\"\n";

	return FS_TestWriteFile( "valve/gameinfo.txt", gameinfo, sizeof( gameinfo ) - 1 );
}

static qboolean TestFindLibrary( void )
{
	fs_dllinfo_t dllinfo;
	static const char dll_data[] = "not a real library";
	static const char direct_dll_data[] = "not a real direct library";

	if( !FS_TestCreateDirectory( "valve" ) ||
		!FS_TestCreateDirectory( "valve/dlls" ) ||
		!FS_TestCreateDirectory( "directlibs" ))
	{
		printf( "failed to create dll lookup directories\n" );
		return false;
	}

	if( !WriteGameInfoFixture() ||
		!FS_TestWriteFile( "valve/dlls/hl." OS_LIB_EXT, dll_data, sizeof( dll_data ) - 1 ) ||
		!FS_TestWriteFile( "directlibs/directlib." OS_LIB_EXT, direct_dll_data, sizeof( direct_dll_data ) - 1 ))
	{
		printf( "failed to write dll lookup fixture\n" );
		return false;
	}

	if( !g_fs.InitStdio( true, ".", "valve", "valve", "" ))
	{
		printf( "InitStdio failed\n" );
		return false;
	}

	g_fs.LoadGameInfo( 0, "" );
	memset( &dllinfo, 0, sizeof( dllinfo ));

	if( !g_fs.FindLibrary( "DLLS/HL", false, &dllinfo ))
	{
		printf( "FindLibrary failed\n" );
		return false;
	}

	if( strcmp( dllinfo.shortPath, "dlls/hl." OS_LIB_EXT ))
	{
		printf( "FindLibrary short path mismatch: %s\n", dllinfo.shortPath );
		return false;
	}

	if( !strstr( dllinfo.fullPath, "valve/dlls/hl." OS_LIB_EXT ) &&
		!strstr( dllinfo.fullPath, "valve\\dlls\\hl." OS_LIB_EXT ))
	{
		printf( "FindLibrary full path mismatch: %s\n", dllinfo.fullPath );
		return false;
	}

	if( dllinfo.encrypted || dllinfo.custom_loader )
	{
		printf( "FindLibrary flags unexpectedly set\n" );
		return false;
	}

	memset( &dllinfo, 0, sizeof( dllinfo ));
	if( !g_fs.FindLibrary( "../valve/dlls/hl." OS_LIB_EXT, false, &dllinfo ))
	{
		printf( "FindLibrary relative path quirk failed\n" );
		return false;
	}

	if( strcmp( dllinfo.shortPath, "dlls/hl." OS_LIB_EXT ))
	{
		printf( "FindLibrary relative short path mismatch: %s\n", dllinfo.shortPath );
		return false;
	}

	memset( &dllinfo, 0, sizeof( dllinfo ));
	if( !g_fs.FindLibrary( "directlibs/directlib", true, &dllinfo ))
	{
		printf( "FindLibrary direct path failed\n" );
		return false;
	}

	if( strcmp( dllinfo.shortPath, "directlibs/directlib." OS_LIB_EXT ))
	{
		printf( "FindLibrary direct short path mismatch: %s\n", dllinfo.shortPath );
		return false;
	}

	if( !strstr( dllinfo.fullPath, "directlibs/directlib." OS_LIB_EXT ) &&
		!strstr( dllinfo.fullPath, "directlibs\\directlib." OS_LIB_EXT ))
	{
		printf( "FindLibrary direct full path mismatch: %s\n", dllinfo.fullPath );
		return false;
	}

	if( dllinfo.encrypted || dllinfo.custom_loader )
	{
		printf( "FindLibrary direct flags unexpectedly set\n" );
		return false;
	}

	if( g_fs.FindLibrary( "", false, &dllinfo ))
	{
		printf( "FindLibrary accepted empty dll name\n" );
		return false;
	}

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/valve/dlls/hl.%s", root, OS_LIB_EXT );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/directlibs/directlib.%s", root, OS_LIB_EXT );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/dlls", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/valve", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/directlibs", root );
	FS_TestRemoveDirectory( path );
	FS_TestRemoveDirectory( root );
}

int main( void )
{
	char testdir[64];
	int result = EXIT_FAILURE;

	if( !LoadFilesystem() )
		return EXIT_FAILURE;

	FS_TestSeedRand();
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_dll" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( TestFindLibrary() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
