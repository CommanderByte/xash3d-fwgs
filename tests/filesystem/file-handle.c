#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"
#include "miniz.h"

#define ZIP_LOCAL_HEADER 0x04034b50U
#define ZIP_CENTRAL_HEADER 0x02014b50U
#define ZIP_EOCD_HEADER 0x06054b50U
#define ZIP_METHOD_DEFLATED 8

fs_test_module_t g_hModule;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

#define EXPECT_TRUE( expr ) \
	do { \
		if( !( expr )) \
		{ \
			printf( "failed expectation at %s:%d: %s\n", __FILE__, __LINE__, #expr ); \
			return false; \
		} \
	} while( 0 )

static qboolean LoadFilesystem( void )
{
	return FS_TestLoadFilesystem( &g_hModule, &g_fs, &g_nullglobals );
}

static qboolean WriteU16( FILE *file, uint16_t value )
{
	unsigned char bytes[2];

	bytes[0] = (unsigned char)( value & 0xff );
	bytes[1] = (unsigned char)(( value >> 8 ) & 0xff );

	return fwrite( bytes, 1, sizeof( bytes ), file ) == sizeof( bytes );
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

static qboolean WriteDeflatedZipFixture( const char *path, const char *name, const char *text )
{
	FILE *file;
	void *payload;
	size_t payload_size = 0;
	size_t name_len = strlen( name );
	size_t text_len = strlen( text );
	uint32_t crc;
	uint32_t central_offset;
	uint32_t central_size;
	long position;
	qboolean ok = true;

	payload = tdefl_compress_mem_to_heap( text, text_len, &payload_size,
		tdefl_create_comp_flags_from_zip_params( MZ_DEFAULT_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY ));
	if( !payload || payload_size > 0xffffffffU || text_len > 0xffffffffU || name_len > 0xffffU )
		return false;

	crc = (uint32_t)mz_crc32( MZ_CRC32_INIT, (const unsigned char *)text, text_len );

	file = fopen( path, "wb" );
	if( !file )
	{
		free( payload );
		return false;
	}

	if( !WriteU32( file, ZIP_LOCAL_HEADER ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, ZIP_METHOD_DEFLATED ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, crc ) ||
		!WriteU32( file, (uint32_t)payload_size ) ||
		!WriteU32( file, (uint32_t)text_len ) ||
		!WriteU16( file, (uint16_t)name_len ) ||
		!WriteU16( file, 0 ) ||
		fwrite( name, 1, name_len, file ) != name_len ||
		fwrite( payload, 1, payload_size, file ) != payload_size )
	{
		ok = false;
	}

	position = ftell( file );
	if( position < 0 )
		ok = false;
	central_offset = (uint32_t)position;

	if( ok && ( !WriteU32( file, ZIP_CENTRAL_HEADER ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, ZIP_METHOD_DEFLATED ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, crc ) ||
		!WriteU32( file, (uint32_t)payload_size ) ||
		!WriteU32( file, (uint32_t)text_len ) ||
		!WriteU16( file, (uint16_t)name_len ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, 0 ) ||
		!WriteU32( file, 0 ) ||
		fwrite( name, 1, name_len, file ) != name_len ))
	{
		ok = false;
	}

	position = ftell( file );
	if( position < 0 )
		ok = false;
	central_size = (uint32_t)position - central_offset;

	if( ok && ( !WriteU32( file, ZIP_EOCD_HEADER ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 1 ) ||
		!WriteU16( file, 1 ) ||
		!WriteU32( file, central_size ) ||
		!WriteU32( file, central_offset ) ||
		!WriteU16( file, 0 )))
	{
		ok = false;
	}

	if( fclose( file ) != 0 )
		ok = false;

	free( payload );
	return ok;
}

static qboolean TestPlainFileHandleOps( void )
{
	static const char text[] = "abcdef\nsecond\r\nthird";
	file_t *file;
	char buffer[32];
	int c;

	file = g_fs.Open( "handle.txt", "wb", false );
	EXPECT_TRUE( file != NULL );

	EXPECT_TRUE( g_fs.Write( file, text, sizeof( text ) - 1 ) == (fs_offset_t)( sizeof( text ) - 1 ));

	EXPECT_TRUE( g_fs.Flush( file ) == 0 );
	EXPECT_TRUE( g_fs.Close( file ) == 0 );

	file = g_fs.Open( "handle.txt", "rb", false );
	EXPECT_TRUE( file != NULL );

	EXPECT_TRUE( g_fs.FileLength( file ) == (fs_offset_t)( sizeof( text ) - 1 ));
	EXPECT_TRUE( g_fs.Tell( file ) == 0 );
	EXPECT_TRUE( !g_fs.Eof( file ));

	memset( buffer, 0, sizeof( buffer ));
	EXPECT_TRUE( g_fs.Read( file, buffer, 3 ) == 3 );
	EXPECT_TRUE( memcmp( buffer, "abc", 3 ) == 0 );
	EXPECT_TRUE( g_fs.Tell( file ) == 3 );

	EXPECT_TRUE( g_fs.Read( file, buffer, 0 ) == 1 );

	EXPECT_TRUE( g_fs.Seek( file, 1, SEEK_SET ) == 0 );
	EXPECT_TRUE( g_fs.Getc( file ) == 'b' );
	EXPECT_TRUE( g_fs.UnGetc( file, 'b' ) == 'b' );
	EXPECT_TRUE( g_fs.UnGetc( file, 'x' ) == EOF );
	EXPECT_TRUE( g_fs.Getc( file ) == 'b' );

	EXPECT_TRUE( g_fs.Seek( file, -1, SEEK_SET ) == -1 );
	EXPECT_TRUE( g_fs.Seek( file, (fs_offset_t)sizeof( text ), SEEK_SET ) == -1 );

	EXPECT_TRUE( g_fs.Seek( file, 0, SEEK_SET ) == 0 );

	memset( buffer, 0, sizeof( buffer ));
	c = g_fs.Gets( file, buffer, sizeof( buffer ));
	EXPECT_TRUE( c == '\n' );
	EXPECT_TRUE( strcmp( buffer, "abcdef" ) == 0 );

	memset( buffer, 0, sizeof( buffer ));
	c = g_fs.Gets( file, buffer, sizeof( buffer ));
	EXPECT_TRUE( c == '\n' );
	EXPECT_TRUE( strcmp( buffer, "second" ) == 0 );

	EXPECT_TRUE( g_fs.Seek( file, -5, SEEK_END ) == 0 );

	memset( buffer, 0, sizeof( buffer ));
	EXPECT_TRUE( g_fs.Read( file, buffer, 5 ) == 5 );
	EXPECT_TRUE( memcmp( buffer, "third", 5 ) == 0 );
	EXPECT_TRUE( g_fs.Eof( file ));

	return g_fs.Close( file ) == 0;
}

static qboolean TestDeflatedFileHandleOps( void )
{
	static const char text[] =
		"deflated handle payload deflated handle payload "
		"deflated handle payload";
	file_t *file;
	char buffer[96];

	EXPECT_TRUE( WriteDeflatedZipFixture( "handle.pk3", "deflated.txt", text ));

	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	file = g_fs.Open( "deflated.txt", "rb", true );
	EXPECT_TRUE( file != NULL );

	EXPECT_TRUE( g_fs.FileLength( file ) == (fs_offset_t)strlen( text ));

	memset( buffer, 0, sizeof( buffer ));
	EXPECT_TRUE( g_fs.Read( file, buffer, 9 ) == 9 );
	EXPECT_TRUE( memcmp( buffer, "deflated", 8 ) == 0 );
	EXPECT_TRUE( g_fs.Tell( file ) == 9 );

	EXPECT_TRUE( g_fs.Seek( file, 0, SEEK_SET ) == 0 );

	memset( buffer, 0, sizeof( buffer ));
	EXPECT_TRUE( g_fs.Read( file, buffer, strlen( text )) == (fs_offset_t)strlen( text ));
	EXPECT_TRUE( memcmp( buffer, text, strlen( text )) == 0 );
	EXPECT_TRUE( g_fs.Eof( file ));

	EXPECT_TRUE( g_fs.Seek( file, 10, SEEK_SET ) == 0 );
	EXPECT_TRUE( g_fs.Seek( file, -4, SEEK_CUR ) == 0 );
	EXPECT_TRUE( g_fs.Tell( file ) == 6 );

	return g_fs.Close( file ) == 0;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ShutdownStdio();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/handle.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/handle.pk3", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_file_handle" );

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

	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	if( TestPlainFileHandleOps() && TestDeflatedFileHandleOps() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
