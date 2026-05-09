#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"

#define IDPACKHEADER (('K'<<24)+('C'<<16)+('A'<<8)+'P')

typedef struct
{
	int ident;
	int dirofs;
	int dirlen;
} test_pak_header_t;

typedef struct
{
	char name[56];
	int filepos;
	int filelen;
} test_pak_file_t;

fs_test_module_t g_hModule;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

static qboolean LoadFilesystem( void )
{
	return FS_TestLoadFilesystem( &g_hModule, &g_fs, &g_nullglobals );
}

static qboolean WritePakFixture( const char *path )
{
	static const char packed_same[] = "packed same";
	static const char packed_only[] = "packed only";
	test_pak_header_t header;
	test_pak_file_t entries[2];
	FILE *file;
	int data_offset = sizeof( header );
	int second_offset = data_offset + (int)sizeof( packed_same ) - 1;
	int dir_offset = second_offset + (int)sizeof( packed_only ) - 1;

	memset( &header, 0, sizeof( header ));
	memset( entries, 0, sizeof( entries ));

	header.ident = IDPACKHEADER;
	header.dirofs = dir_offset;
	header.dirlen = sizeof( entries );

	strncpy( entries[0].name, "same.txt", sizeof( entries[0].name ) - 1 );
	entries[0].filepos = data_offset;
	entries[0].filelen = sizeof( packed_same ) - 1;

	strncpy( entries[1].name, "onlypak.txt", sizeof( entries[1].name ) - 1 );
	entries[1].filepos = second_offset;
	entries[1].filelen = sizeof( packed_only ) - 1;

	file = fopen( path, "wb" );
	if( !file )
		return false;

	if( fwrite( &header, 1, sizeof( header ), file ) != sizeof( header ) ||
		fwrite( packed_same, 1, sizeof( packed_same ) - 1, file ) != sizeof( packed_same ) - 1 ||
		fwrite( packed_only, 1, sizeof( packed_only ) - 1, file ) != sizeof( packed_only ) - 1 ||
		fwrite( entries, 1, sizeof( entries ), file ) != sizeof( entries ))
	{
		fclose( file );
		return false;
	}

	fclose( file );
	return true;
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

static qboolean TestPakAndLoosePrecedence( void )
{
	static const char loose_same[] = "loose same";

	if( !WritePakFixture( "pak0.pak" ))
	{
		printf( "failed to write pak fixture\n" );
		return false;
	}

	if( !FS_TestWriteFile( "same.txt", loose_same, sizeof( loose_same ) - 1 ))
	{
		printf( "failed to write loose fixture\n" );
		return false;
	}

	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	if( !CheckLoadedText( "same.txt", loose_same ))
		return false;

	if( !CheckLoadedText( "onlypak.txt", "packed only" ))
		return false;

	return true;
}

static qboolean TestDirectPakMount( void )
{
	void *first_mount;
	void *second_mount;

	if( !WritePakFixture( "direct.PAK" ))
	{
		printf( "failed to write direct pak fixture\n" );
		return false;
	}

	first_mount = g_fs.MountArchive_Fullpath( "direct.PAK", FS_GAMEDIR_PATH );
	if( !first_mount )
	{
		printf( "failed to mount direct pak fixture\n" );
		return false;
	}

	second_mount = g_fs.MountArchive_Fullpath( "direct.PAK", FS_GAMEDIR_PATH );
	if( second_mount != first_mount )
	{
		printf( "direct pak mount was not idempotent\n" );
		return false;
	}

	if( !CheckLoadedText( "onlypak.txt", "packed only" ))
		return false;

	if( g_fs.MountArchive_Fullpath( "unsupported.vpk", FS_GAMEDIR_PATH ))
	{
		printf( "unsupported archive unexpectedly mounted\n" );
		return false;
	}

	g_fs.ClearSearchPath();
	FS_TestRemoveFile( "direct.PAK" );
	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ClearSearchPath();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/same.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/pak0.pak", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/direct.PAK", root );
	FS_TestRemoveFile( path );
	FS_TestRemoveDirectory( root );
}

int main( void )
{
	char testdir[64];
	int result = EXIT_FAILURE;

	if( !LoadFilesystem() )
		return EXIT_FAILURE;

	FS_TestSeedRand();
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_archive" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( TestDirectPakMount() && TestPakAndLoosePrecedence() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
