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

static qboolean CheckLoadedText( const char *path, const char *expected )
{
	fs_offset_t len;
	byte *data = g_fs.LoadFile( path, &len, true );

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

static qboolean TestPk3DirMount( void )
{
	static const char pk3dir_only[] = "pk3dir only";
	static const char loose_same[] = "loose same";
	static const char pk3dir_same[] = "pk3dir same";

	if( !FS_TestCreateDirectory( "test.pk3dir" ))
	{
		printf( "failed to create pk3dir fixture\n" );
		return false;
	}

	if( !FS_TestWriteFile( "test.pk3dir/onlydir.txt", pk3dir_only, sizeof( pk3dir_only ) - 1 ) ||
		!FS_TestWriteFile( "test.pk3dir/same.txt", pk3dir_same, sizeof( pk3dir_same ) - 1 ) ||
		!FS_TestWriteFile( "same.txt", loose_same, sizeof( loose_same ) - 1 ))
	{
		printf( "failed to write pk3dir fixture files\n" );
		return false;
	}

	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	if( !CheckLoadedText( "onlydir.txt", "pk3dir only" ))
		return false;

	if( !CheckLoadedText( "same.txt", "loose same" ))
		return false;

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ClearSearchPath();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/same.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/test.pk3dir/onlydir.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/test.pk3dir/same.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/test.pk3dir", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_pk3dir" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( TestPk3DirMount() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
