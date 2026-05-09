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
		"title \"Filesystem Direct Path Test\"\n"
		"basedir \"valve\"\n"
		"gamedir \"valve\"\n"
		"gamedll \"dlls/hl.dll\"\n";

	if( !FS_TestCreateDirectory( "valve" ))
	{
		printf( "failed to create valve fixture directory\n" );
		return false;
	}

	if( !FS_TestWriteFile( "valve/gameinfo.txt", gameinfo, sizeof( gameinfo ) - 1 ))
	{
		printf( "failed to write gameinfo fixture\n" );
		return false;
	}

	return true;
}

static qboolean CheckLoadedData( const char *path, const char *expected )
{
	fs_offset_t len;
	byte *data = g_fs.LoadFile( path, &len, false );

	if( !data )
	{
		printf( "LoadFile failed for %s\n", path );
		return false;
	}

	if( len != (fs_offset_t)strlen( expected ) || memcmp( data, expected, (size_t)len ))
	{
		printf( "LoadFile data mismatch for %s\n", path );
		free( data );
		return false;
	}

	free( data );
	return true;
}

static qboolean TestDirectPathReads( void )
{
	static const char magic[] = "direct-path-magic";

	if( !FS_TestWriteFile( "direct.txt", magic, sizeof( magic ) - 1 ))
	{
		printf( "failed to write direct path fixture\n" );
		return false;
	}

	if( g_fs.LoadFile( "../direct.txt", NULL, false ))
	{
		printf( "direct path loaded while disabled\n" );
		return false;
	}

	g_fs.AllowDirectPaths( true );

	if( !CheckLoadedData( "../direct.txt", magic ))
	{
		g_fs.AllowDirectPaths( false );
		return false;
	}

	g_fs.AllowDirectPaths( false );

	if( g_fs.LoadFile( "../direct.txt", NULL, false ))
	{
		printf( "direct path loaded after reset\n" );
		return false;
	}

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/direct.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_direct" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( WriteGameInfoFixture() &&
		g_fs.InitStdio( true, ".", "valve", "valve", "" ) &&
		TestDirectPathReads() )
	{
		result = EXIT_SUCCESS;
	}

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
