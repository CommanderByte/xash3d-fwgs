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

static qboolean CheckFileContents( const char *path, const void *buf, fs_offset_t size )
{
	fs_offset_t len;
	byte *data;

	data = g_fs.LoadFile( path, &len, true );
	if( !data )
	{
		printf( "LoadFile fail\n" );
		return false;
	}

	if( len != size )
	{
		printf( "LoadFile sizeof fail\n" );
		free( data );
		return false;
	}

	if( memcmp( data, buf, size ))
	{
		printf( "LoadFile magic fail\n" );
		free( data );
		return false;
	}

	free( data );
	return true;
}

static qboolean TestCaseinsensitive( void )
{
	file_t *f1;
	int magic = rand();
	int nested_magic = rand();
	int cache_magic = rand();

	// create game dir for us
	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	// create some files first and write data
	f1 = g_fs.Open( "FOO/Bar.bin", "wb", true );
	g_fs.Write( f1, &magic, sizeof( magic ));
	g_fs.Close( f1 );

	// try to search it with different file name
	if( !g_fs.FileExists( "fOO/baR.bin", true ))
	{
		printf( "FileExists fail\n" );
		return false;
	}

	// create a file directly, to check if cache can re-read
	if( !FS_TestWriteFile( "FOO/Baz.bin", &magic, sizeof( magic )))
	{
		printf( "direct file write fail\n" );
		return false;
	}

	// try to open first file back but with different file name case
	if( !CheckFileContents( "foo/bar.BIN", &magic, sizeof( magic )))
		return false;

	// try to open second file that we created directly
	if( !CheckFileContents( "Foo/BaZ.Bin", &magic, sizeof( magic )))
		return false;

	g_fs.Delete( "foo/Baz.biN" );
	g_fs.Delete( "foo/bar.bin" );
	g_fs.Delete( "Foo" );

	return true;
}

static qboolean TestNestedCaseFixing( void )
{
	file_t *f;
	int magic = rand();

	f = g_fs.Open( "Outer/Inner/Deep.bin", "wb", true );
	if( !f )
	{
		printf( "nested write open fail\n" );
		return false;
	}

	g_fs.Write( f, &magic, sizeof( magic ));
	g_fs.Close( f );

	if( !g_fs.FileExists( "outer/inner/deep.BIN", true ))
	{
		printf( "nested FileExists fail\n" );
		return false;
	}

	if( !CheckFileContents( "OUTER/INNER/DEEP.bin", &magic, sizeof( magic )))
		return false;

	g_fs.Delete( "outer/inner/deep.bin" );
	g_fs.Delete( "outer/inner" );
	g_fs.Delete( "outer" );

	return true;
}

static qboolean TestCacheRefreshForDirectFile( void )
{
	int magic = rand();

	if( !g_fs.FileExists( "Live/Direct.bin", true ))
	{
		file_t *f = g_fs.Open( "Live/Seed.bin", "wb", true );
		if( !f )
		{
			printf( "cache seed open fail\n" );
			return false;
		}

		g_fs.Write( f, &magic, sizeof( magic ));
		g_fs.Close( f );
	}

	if( !FS_TestWriteFile( "Live/Direct.bin", &magic, sizeof( magic )))
	{
		printf( "cache direct file write fail\n" );
		return false;
	}

	if( !CheckFileContents( "live/direct.BIN", &magic, sizeof( magic )))
		return false;

	g_fs.Delete( "Live/Direct.bin" );
	g_fs.Delete( "Live/Seed.bin" );
	g_fs.Delete( "Live" );

	return true;
}

static qboolean TestWritePathCreatesDirectories( void )
{
	file_t *f;
	int magic = rand();

	f = g_fs.Open( "Created/By/Write.bin", "wb", true );
	if( !f )
	{
		printf( "write path create open fail\n" );
		return false;
	}

	g_fs.Write( f, &magic, sizeof( magic ));
	g_fs.Close( f );

	if( !CheckFileContents( "created/by/write.BIN", &magic, sizeof( magic )))
		return false;

	g_fs.Delete( "Created/By/Write.bin" );
	g_fs.Delete( "Created/By" );
	g_fs.Delete( "Created" );

	return true;
}

static qboolean TestRejectedPaths( void )
{
	if( g_fs.Open( "../escape.bin", "wb", true ) )
	{
		printf( "parent path write unexpectedly allowed\n" );
		return false;
	}

	if( g_fs.Open( "/escape.bin", "rb", true ) )
	{
		printf( "absolute path read unexpectedly allowed\n" );
		return false;
	}

	if( g_fs.Open( "C:/escape.bin", "rb", true ) )
	{
		printf( "colon path read unexpectedly allowed\n" );
		return false;
	}

	if( g_fs.FileExists( "../escape.bin", true ))
	{
		printf( "parent path exists unexpectedly allowed\n" );
		return false;
	}

	return true;
}

int main( void )
{
	char testdir[64];

	if( !LoadFilesystem() )
		return EXIT_FAILURE;

	FS_TestSeedRand();
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_case" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
		return EXIT_FAILURE;

	if( !TestCaseinsensitive())
		return EXIT_FAILURE;

	if( !TestNestedCaseFixing())
		return EXIT_FAILURE;

	if( !TestCacheRefreshForDirectFile())
		return EXIT_FAILURE;

	if( !TestWritePathCreatesDirectories())
		return EXIT_FAILURE;

	if( !TestRejectedPaths())
		return EXIT_FAILURE;

	g_fs.SetCurrentDirectory( ".." );
	FS_TestRemoveDirectory( testdir );

	printf( "success\n" );

	return EXIT_SUCCESS;
}
