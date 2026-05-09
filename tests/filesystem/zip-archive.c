#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"
#include "miniz.h"

#define ZIP_LOCAL_HEADER 0x04034b50U
#define ZIP_CENTRAL_HEADER 0x02014b50U
#define ZIP_EOCD_HEADER 0x06054b50U
#define ZIP_METHOD_STORED 0
#define ZIP_METHOD_DEFLATED 8
#define ZIP_METHOD_UNSUPPORTED 99

typedef struct
{
	const char *name;
	const void *data;
	uint16_t method;
	uint32_t crc32;
	uint32_t local_offset;
	uint32_t compressed_size;
	uint32_t uncompressed_size;
} zip_entry_t;

fs_test_module_t g_hModule;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

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

static qboolean WriteZipLocalEntry( FILE *file, zip_entry_t *entry )
{
	size_t name_len = strlen( entry->name );
	long offset = ftell( file );

	if( offset < 0 || name_len > 0xffff )
		return false;

	entry->local_offset = (uint32_t)offset;

	if( !WriteU32( file, ZIP_LOCAL_HEADER ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, entry->method ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, entry->crc32 ) ||
		!WriteU32( file, entry->compressed_size ) ||
		!WriteU32( file, entry->uncompressed_size ) ||
		!WriteU16( file, (uint16_t)name_len ) ||
		!WriteU16( file, 0 ))
		return false;

	if( fwrite( entry->name, 1, name_len, file ) != name_len )
		return false;

	return fwrite( entry->data, 1, entry->compressed_size, file ) == entry->compressed_size;
}

static qboolean WriteZipCentralEntry( FILE *file, const zip_entry_t *entry )
{
	size_t name_len = strlen( entry->name );

	if( name_len > 0xffff )
		return false;

	if( !WriteU32( file, ZIP_CENTRAL_HEADER ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 20 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, entry->method ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, entry->crc32 ) ||
		!WriteU32( file, entry->compressed_size ) ||
		!WriteU32( file, entry->uncompressed_size ) ||
		!WriteU16( file, (uint16_t)name_len ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU32( file, 0 ) ||
		!WriteU32( file, entry->local_offset ))
		return false;

	return fwrite( entry->name, 1, name_len, file ) == name_len;
}

static qboolean WriteZipFixture( const char *path )
{
	static const char stored_data[] = "stored zip payload";
	static const char deflated_data[] =
		"deflated zip payload deflated zip payload deflated zip payload "
		"deflated zip payload deflated zip payload";
	static const char unsupported_data[] = "unsupported zip payload";
	zip_entry_t entries[3];
	void *deflated_payload;
	size_t deflated_size = 0;
	uint32_t central_offset;
	uint32_t central_size;
	long position;
	FILE *file;
	qboolean ok = true;

	deflated_payload = tdefl_compress_mem_to_heap( deflated_data, sizeof( deflated_data ) - 1, &deflated_size,
		tdefl_create_comp_flags_from_zip_params( MZ_DEFAULT_COMPRESSION, -MZ_DEFAULT_WINDOW_BITS, MZ_DEFAULT_STRATEGY ));

	if( !deflated_payload || deflated_size > 0xffffffffU )
	{
		printf( "failed to deflate zip fixture data\n" );
		return false;
	}

	memset( entries, 0, sizeof( entries ));

	entries[0].name = "stored.txt";
	entries[0].data = stored_data;
	entries[0].method = ZIP_METHOD_STORED;
	entries[0].crc32 = (uint32_t)mz_crc32( MZ_CRC32_INIT, (const unsigned char *)stored_data, sizeof( stored_data ) - 1 );
	entries[0].compressed_size = sizeof( stored_data ) - 1;
	entries[0].uncompressed_size = sizeof( stored_data ) - 1;

	entries[1].name = "deflated.txt";
	entries[1].data = deflated_payload;
	entries[1].method = ZIP_METHOD_DEFLATED;
	entries[1].crc32 = (uint32_t)mz_crc32( MZ_CRC32_INIT, (const unsigned char *)deflated_data, sizeof( deflated_data ) - 1 );
	entries[1].compressed_size = (uint32_t)deflated_size;
	entries[1].uncompressed_size = sizeof( deflated_data ) - 1;

	entries[2].name = "unsupported.txt";
	entries[2].data = unsupported_data;
	entries[2].method = ZIP_METHOD_UNSUPPORTED;
	entries[2].crc32 = (uint32_t)mz_crc32( MZ_CRC32_INIT, (const unsigned char *)unsupported_data, sizeof( unsupported_data ) - 1 );
	entries[2].compressed_size = sizeof( unsupported_data ) - 1;
	entries[2].uncompressed_size = sizeof( unsupported_data ) - 1;

	file = fopen( path, "wb" );
	if( !file )
	{
		free( deflated_payload );
		return false;
	}

	if( !WriteZipLocalEntry( file, &entries[0] ) ||
		!WriteZipLocalEntry( file, &entries[1] ) ||
		!WriteZipLocalEntry( file, &entries[2] ))
	{
		printf( "failed to write zip local entries\n" );
		ok = false;
	}

	position = ftell( file );
	if( position < 0 )
		ok = false;
	central_offset = (uint32_t)position;

	if( ok && ( !WriteZipCentralEntry( file, &entries[0] ) ||
		!WriteZipCentralEntry( file, &entries[1] ) ||
		!WriteZipCentralEntry( file, &entries[2] )))
	{
		printf( "failed to write zip central directory\n" );
		ok = false;
	}

	position = ftell( file );
	if( position < 0 )
		ok = false;
	central_size = (uint32_t)position - central_offset;

	if( ok && ( !WriteU32( file, ZIP_EOCD_HEADER ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 0 ) ||
		!WriteU16( file, 3 ) ||
		!WriteU16( file, 3 ) ||
		!WriteU32( file, central_size ) ||
		!WriteU32( file, central_offset ) ||
		!WriteU16( file, 0 )))
	{
		printf( "failed to write zip end record\n" );
		ok = false;
	}

	if( fclose( file ) != 0 )
		ok = false;

	free( deflated_payload );
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

static qboolean TestZipArchiveLoads( void )
{
	static const char bad_zip[] = "not a zip";

	if( !FS_TestWriteFile( "bad.pk3", bad_zip, sizeof( bad_zip ) - 1 ))
		return false;

	if( g_fs.MountArchive_Fullpath( "bad.pk3", FS_GAMEDIR_PATH ))
	{
		printf( "corrupted zip unexpectedly mounted\n" );
		return false;
	}

	if( !WriteZipFixture( "test.pk3" ))
		return false;

	g_fs.AddGameDirectory( "./", FS_GAMEDIR_PATH );

	if( !CheckLoadedText( "stored.txt", "stored zip payload" ))
		return false;

	if( !CheckLoadedText( "deflated.txt",
		"deflated zip payload deflated zip payload deflated zip payload "
		"deflated zip payload deflated zip payload" ))
		return false;

	if( g_fs.LoadFile( "unsupported.txt", NULL, true ))
	{
		printf( "unsupported compressed zip file unexpectedly loaded\n" );
		return false;
	}

	return true;
}

static void CleanupFixture( const char *root )
{
	char path[256];

	g_fs.ClearSearchPath();
	g_fs.SetCurrentDirectory( ".." );

	snprintf( path, sizeof( path ), "%s/test.pk3", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/bad.pk3", root );
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
	FS_TestMakeUniqueName( testdir, sizeof( testdir ), "fs_zip" );

	if( !FS_TestCreateDirectory( testdir ))
		return EXIT_FAILURE;

	if( !g_fs.SetCurrentDirectory( testdir ))
	{
		FS_TestRemoveDirectory( testdir );
		return EXIT_FAILURE;
	}

	if( TestZipArchiveLoads() )
		result = EXIT_SUCCESS;

	CleanupFixture( testdir );

	if( result == EXIT_SUCCESS )
		printf( "success\n" );

	return result;
}
