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

static qboolean WriteGameInfoFixture( const char *dir )
{
	char path[256];
	static const char gameinfo[] =
		"title \"Rodir Test\"\n"
		"basedir \"valve\"\n"
		"gamedll \"dlls/hl.dll\"\n";

	snprintf( path, sizeof( path ), "%s/gameinfo.txt", dir );
	if( !FS_TestWriteFile( path, gameinfo, sizeof( gameinfo ) - 1 ))
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

static qboolean WriteRodirFixture( void )
{
	static const char rw_shared[] = "rw shared";
	static const char ro_shared[] = "ro shared";
	static const char ro_only[] = "ro only";

	if( !FS_TestCreateDirectory( "valve" ) ||
		!FS_TestCreateDirectory( "ro" ) ||
		!FS_TestCreateDirectory( "ro/valve" ))
	{
		printf( "failed to create rodir fixture directories\n" );
		return false;
	}

	if( !WriteGameInfoFixture( "valve" ) ||
		!WriteGameInfoFixture( "ro/valve" ))
		return false;

	if( !FS_TestWriteFile( "valve/shared.txt", rw_shared, sizeof( rw_shared ) - 1 ) ||
		!FS_TestWriteFile( "ro/valve/shared.txt", ro_shared, sizeof( ro_shared ) - 1 ) ||
		!FS_TestWriteFile( "ro/valve/readonly.txt", ro_only, sizeof( ro_only ) - 1 ))
	{
		printf( "failed to write rodir fixture files\n" );
		return false;
	}

	return true;
}

static qboolean TestRodirPrecedence( void )
{
	file_t *file;
	static const char created[] = "created in writable root";

	if( !g_fs.InitStdio( true, ".", "valve", "valve", "ro" ))
	{
		printf( "InitStdio failed\n" );
		return false;
	}

	g_fs.LoadGameInfo( 0, "" );

	if( !CheckLoadedText( "shared.txt", "rw shared", false ))
		return false;

	if( !CheckLoadedText( "readonly.txt", "ro only", false ))
		return false;

	if( !CheckLoadedText( "readonly.txt", "ro only", true ))
		return false;

	file = g_fs.Open( "created.txt", "wb", true );
	if( !file )
	{
		printf( "failed to open writable-root file\n" );
		return false;
	}

	if( g_fs.Write( file, created, sizeof( created ) - 1 ) != sizeof( created ) - 1 )
	{
		g_fs.Close( file );
		printf( "failed to write writable-root file\n" );
		return false;
	}
	g_fs.Close( file );

	if( !g_fs.SysFileExists( "valve/created.txt" ))
	{
		printf( "writable-root file was not created in rw gamedir\n" );
		return false;
	}

	if( g_fs.SysFileExists( "ro/valve/created.txt" ))
	{
		printf( "writable-root file was unexpectedly created in rodir\n" );
		return false;
	}

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/valve/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/created.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/valve/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/ro/valve/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/ro/valve/readonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/ro/valve/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/ro/valve", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/ro", root );
	FS_TestRemoveDirectory( path );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_rodir" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( WriteRodirFixture() && TestRodirPrecedence() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
