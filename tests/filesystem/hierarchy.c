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

static qboolean WriteGameInfoFixture( const char *dir, const char *title, const char *basedir )
{
	char path[256];
	char gameinfo[512];

	snprintf( gameinfo, sizeof( gameinfo ),
		"title \"%s\"\n"
		"basedir \"%s\"\n"
		"gamedll \"dlls/hl.dll\"\n",
		title, basedir );

	snprintf( path, sizeof( path ), "%s/gameinfo.txt", dir );
	if( !FS_TestWriteFile( path, gameinfo, strlen( gameinfo )))
	{
		printf( "failed to write %s\n", path );
		return false;
	}

	return true;
}

static qboolean CheckLoadedText( const char *path, const char *expected, qboolean gamedironly )
{
	fs_offset_t len;
	byte *data = g_fs.LoadFile( path, &len, gamedironly );

	if( !data )
	{
		printf( "LoadFile failed for %s\n", path );
		return false;
	}

	if( len != (fs_offset_t)strlen( expected ) || memcmp( data, expected, (size_t)len ))
	{
		printf( "LoadFile text mismatch for %s\n", path );
		free( data );
		return false;
	}

	free( data );
	return true;
}

static qboolean WriteHierarchyFixture( void )
{
	static const char base_shared[] = "base shared";
	static const char mod_shared[] = "mod shared";
	static const char base_only[] = "base only";

	if( !FS_TestCreateDirectory( "valve" ) ||
		!FS_TestCreateDirectory( "mod" ))
	{
		printf( "failed to create game hierarchy directories\n" );
		return false;
	}

	if( !WriteGameInfoFixture( "valve", "Base Game", "valve" ) ||
		!WriteGameInfoFixture( "mod", "Mod Game", "valve" ))
		return false;

	if( !FS_TestWriteFile( "valve/shared.txt", base_shared, sizeof( base_shared ) - 1 ) ||
		!FS_TestWriteFile( "mod/shared.txt", mod_shared, sizeof( mod_shared ) - 1 ) ||
		!FS_TestWriteFile( "valve/baseonly.txt", base_only, sizeof( base_only ) - 1 ))
	{
		printf( "failed to write hierarchy fixture files\n" );
		return false;
	}

	return true;
}

static qboolean TestGameHierarchyPrecedence( void )
{
	if( !g_fs.InitStdio( true, ".", "valve", "mod", "" ))
	{
		printf( "InitStdio failed\n" );
		return false;
	}

	g_fs.LoadGameInfo( 0, "" );

	if( !CheckLoadedText( "shared.txt", "mod shared", false ))
		return false;

	if( !CheckLoadedText( "baseonly.txt", "base only", false ))
		return false;

	if( g_fs.LoadFile( "baseonly.txt", NULL, true ))
	{
		printf( "gamedironly loaded base directory file\n" );
		return false;
	}

	if( !CheckLoadedText( "shared.txt", "mod shared", true ))
		return false;

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/valve/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/baseonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_hierarchy" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( WriteHierarchyFixture() && TestGameHierarchyPrecedence() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
