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

static qboolean ExpectResult(
	const search_t *search,
	int index,
	const char *expected )
{
	if( !search )
	{
		printf( "missing search result for %s\n", expected );
		return false;
	}

	if( index >= search->numfilenames )
	{
		printf( "missing search index %d for %s\n", index, expected );
		return false;
	}

	if( strcmp( search->filenames[index], expected ))
	{
		printf( "search result %d mismatch: got %s expected %s\n",
			index, search->filenames[index], expected );
		return false;
	}

	return true;
}

static qboolean WriteSearchFixture( void )
{
	static const char alpha[] = "base alpha";
	static const char same_base[] = "base duplicate";
	static const char same_mod[] = "mod duplicate";
	static const char zeta[] = "mod zeta";
	static const char hidden[] = "not txt";

	if( !FS_TestCreateDirectory( "base" ) ||
		!FS_TestCreateDirectory( "mod" ))
		return false;

	return FS_TestWriteFile( "base/alpha.txt", alpha, sizeof( alpha ) - 1 ) &&
		FS_TestWriteFile( "base/same.txt", same_base, sizeof( same_base ) - 1 ) &&
		FS_TestWriteFile( "mod/same.txt", same_mod, sizeof( same_mod ) - 1 ) &&
		FS_TestWriteFile( "mod/zeta.txt", zeta, sizeof( zeta ) - 1 ) &&
		FS_TestWriteFile( "mod/hidden.bin", hidden, sizeof( hidden ) - 1 );
}

static qboolean TestSearchOrderingAndDuplicates( void )
{
	search_t *search;

	g_fs.AddGameDirectory( "base/", FS_NOWRITE_PATH );
	g_fs.AddGameDirectory( "mod/", FS_GAMEDIR_PATH );

	search = g_fs.Search( "*.txt", true, false );
	if( !search )
		return false;

	if( search->numfilenames != 3 )
	{
		printf( "expected 3 search results, got %d\n", search->numfilenames );
		return false;
	}

	return ExpectResult( search, 0, "alpha.txt" ) &&
		ExpectResult( search, 1, "same.txt" ) &&
		ExpectResult( search, 2, "zeta.txt" );
}

static qboolean TestSearchGamedirOnlyFiltering( void )
{
	search_t *search = g_fs.Search( "*.txt", true, true );

	if( !search )
		return false;

	if( search->numfilenames != 2 )
	{
		printf( "expected 2 gamedir-only search results, got %d\n", search->numfilenames );
		return false;
	}

	return ExpectResult( search, 0, "same.txt" ) &&
		ExpectResult( search, 1, "zeta.txt" );
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/base/alpha.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/base/same.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/same.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/zeta.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/hidden.bin", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/base", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_search_results" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( !g_fs.InitStdio( true, ".", "", "", "" ))
	{
		CleanupFixture( testdir );
		return EXIT_FAILURE;
	}

	if( WriteSearchFixture() &&
		TestSearchOrderingAndDuplicates() &&
		TestSearchGamedirOnlyFiltering() )
	{
		result = EXIT_SUCCESS;
	}

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
