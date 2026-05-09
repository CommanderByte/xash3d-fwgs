#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"
#include "wadfile.h"

#define IDPACKHEADER (('K'<<24)+('C'<<16)+('A'<<8)+'P')

fs_test_module_t g_hModule;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

static qboolean LoadFilesystem( void )
{
	return FS_TestLoadFilesystem( &g_hModule, &g_fs, &g_nullglobals );
}

static qboolean WriteU32( FILE *file, uint32_t value )
{
	unsigned char bytes[4];

	bytes[0] = (unsigned char)( value & 0xff );
	bytes[1] = (unsigned char)(( value >> 8 ) & 0xff );
	bytes[2] = (unsigned char)(( value >> 16 ) & 0xff );
	bytes[3] = (unsigned char)(( value >> 24 ) & 0xff );

	return fwrite( bytes, 1, sizeof( bytes ), file ) == sizeof( bytes );
}

static qboolean WriteWadLumpName( FILE *file, const char *name )
{
	char lumpname[WAD3_NAMELEN];

	memset( lumpname, 0, sizeof( lumpname ));
	strncpy( lumpname, name, sizeof( lumpname ) - 1 );

	return fwrite( lumpname, 1, sizeof( lumpname ), file ) == sizeof( lumpname );
}

static qboolean WriteWadFixture( const char *path, const char *lumpname, const char *payload )
{
	FILE *file = fopen( path, "wb" );
	uint32_t payload_size = (uint32_t)strlen( payload );
	uint32_t payload_offset = sizeof( dwadinfo_t );
	uint32_t infotable_offset = payload_offset + payload_size;
	qboolean ok = true;

	if( !file )
		return false;

	if( !WriteU32( file, IDWAD3HEADER ) ||
		!WriteU32( file, 1 ) ||
		!WriteU32( file, infotable_offset ) ||
		fwrite( payload, 1, payload_size, file ) != payload_size ||
		!WriteU32( file, payload_offset ) ||
		!WriteU32( file, payload_size ) ||
		!WriteU32( file, payload_size ) ||
		fputc( TYP_SCRIPT, file ) == EOF ||
		fputc( ATTR_NONE, file ) == EOF ||
		fputc( 0, file ) == EOF ||
		fputc( 0, file ) == EOF ||
		!WriteWadLumpName( file, lumpname ))
		ok = false;

	if( fclose( file ) != 0 )
		ok = false;

	return ok;
}

static qboolean ReadFixtureFile( const char *path, byte **data, size_t *size )
{
	FILE *file = fopen( path, "rb" );
	long length;

	*data = NULL;
	*size = 0;

	if( !file )
		return false;

	if( fseek( file, 0, SEEK_END ) != 0 )
	{
		fclose( file );
		return false;
	}

	length = ftell( file );
	if( length < 0 || fseek( file, 0, SEEK_SET ) != 0 )
	{
		fclose( file );
		return false;
	}

	*data = (byte *)malloc( (size_t)length );
	if( !*data )
	{
		fclose( file );
		return false;
	}

	*size = (size_t)length;
	if( fread( *data, 1, *size, file ) != *size )
	{
		free( *data );
		*data = NULL;
		*size = 0;
		fclose( file );
		return false;
	}

	fclose( file );
	return true;
}

static qboolean WritePakWithWadFixture( const char *pakpath, const char *wadpath, const char *entryname )
{
	byte *wad_data;
	size_t wad_size;
	FILE *file;
	char name[56];
	uint32_t data_offset = 12;
	uint32_t dir_offset;
	qboolean ok = true;

	if( !ReadFixtureFile( wadpath, &wad_data, &wad_size ))
		return false;

	if( wad_size > 0xffffffffU )
	{
		free( wad_data );
		return false;
	}

	dir_offset = data_offset + (uint32_t)wad_size;
	file = fopen( pakpath, "wb" );
	if( !file )
	{
		free( wad_data );
		return false;
	}

	memset( name, 0, sizeof( name ));
	strncpy( name, entryname, sizeof( name ) - 1 );

	if( !WriteU32( file, IDPACKHEADER ) ||
		!WriteU32( file, dir_offset ) ||
		!WriteU32( file, 64 ) ||
		fwrite( wad_data, 1, wad_size, file ) != wad_size ||
		fwrite( name, 1, sizeof( name ), file ) != sizeof( name ) ||
		!WriteU32( file, data_offset ) ||
		!WriteU32( file, (uint32_t)wad_size ))
		ok = false;

	if( fclose( file ) != 0 )
		ok = false;

	free( wad_data );
	return ok;
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

static qboolean ExpectMissingLoad( const char *path )
{
	fs_offset_t len = -1;
	byte *data = g_fs.LoadFile( path, &len, true );

	if( data )
	{
		printf( "LoadFile unexpectedly succeeded for %s\n", path );
		free( data );
		return false;
	}

	return true;
}

static qboolean ExpectSearchResult( const search_t *search, int index, const char *expected )
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

static qboolean CheckSearchResults( const char *pattern, const char **expected, int count )
{
	search_t *search = g_fs.Search( pattern, true, true );
	int i;

	if( !search )
	{
		printf( "Search failed for %s\n", pattern );
		return false;
	}

	if( search->numfilenames != count )
	{
		printf( "expected %d search results for %s, got %d\n",
			count, pattern, search->numfilenames );
		return false;
	}

	for( i = 0; i < count; i++ )
	{
		if( !ExpectSearchResult( search, i, expected[i] ))
			return false;
	}

	return true;
}

static qboolean TestWadLoads( void )
{
	static const char *all_txt_results[] =
	{
		"/packed.txt",
		"/probe.txt"
	};
	static const char *raw_txt_results[] =
	{
		"raw/probe.txt"
	};

	if( !WriteWadFixture( "raw.wad", "probe", "raw wad payload" ) ||
		!WriteWadFixture( "inside.wad", "packed", "packed wad payload" ) ||
		!WritePakWithWadFixture( "pak0.pak", "inside.wad", "inside.wad" ))
	{
		printf( "failed to create wad fixture\n" );
		return false;
	}

	FS_TestRemoveFile( "inside.wad" );
	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	if( !CheckLoadedText( "probe.txt", "raw wad payload" ))
		return false;

	if( !CheckLoadedText( "packed.txt", "packed wad payload" ))
		return false;

	if( !CheckLoadedText( "raw.wad/probe.txt", "raw wad payload" ))
		return false;

	if( !CheckLoadedText( "PROBE.TXT", "raw wad payload" ))
		return false;

	if( !CheckLoadedText( "probe", "raw wad payload" ))
		return false;

	if( !ExpectMissingLoad( "other.wad/probe.txt" ))
		return false;

	if( !ExpectMissingLoad( "probe.bin" ))
		return false;

	return CheckSearchResults( "*.txt", all_txt_results, 2 ) &&
		CheckSearchResults( "raw.wad/*.txt", raw_txt_results, 1 );
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ClearSearchPath();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/raw.wad", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/inside.wad", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/pak0.pak", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_wad" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( TestWadLoads() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
