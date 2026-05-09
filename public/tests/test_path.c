#include <stdlib.h>
#include <stdio.h>

#include "crtlib.h"

static int ExpectString( const char *actual, const char *expected, int code )
{
	if( Q_strcmp( actual, expected ))
	{
		printf( "expected \"%s\", got \"%s\"\n", expected, actual );
		return code;
	}

	return 0;
}

static int Test_FileWithoutPath( void )
{
	int result;

	result = ExpectString( COM_FileWithoutPath( "dir/file.txt" ), "file.txt", 1 );
	if( result ) return result;

	result = ExpectString( COM_FileWithoutPath( "dir\\file.txt" ), "file.txt", 2 );
	if( result ) return result;

	result = ExpectString( COM_FileWithoutPath( "c:/games/valve/pak0.pak" ), "pak0.pak", 3 );
	if( result ) return result;

	result = ExpectString( COM_FileWithoutPath( "drive:file.txt" ), "file.txt", 4 );
	if( result ) return result;

	result = ExpectString( COM_FileWithoutPath( "dir/" ), "", 5 );
	if( result ) return result;

	return ExpectString( COM_FileWithoutPath( "file.txt" ), "file.txt", 6 );
}

static int Test_StripExtension( void )
{
	char path[64];
	int result;

	Q_strncpy( path, "dir/file.ext", sizeof( path ));
	COM_StripExtension( path );
	result = ExpectString( path, "dir/file", 1 );
	if( result ) return result;

	Q_strncpy( path, "dir.ext/file", sizeof( path ));
	COM_StripExtension( path );
	result = ExpectString( path, "dir.ext/file", 2 );
	if( result ) return result;

	Q_strncpy( path, ".nomedia", sizeof( path ));
	COM_StripExtension( path );
	result = ExpectString( path, ".nomedia", 3 );
	if( result ) return result;

	Q_strncpy( path, "file.", sizeof( path ));
	COM_StripExtension( path );
	result = ExpectString( path, "file", 4 );
	if( result ) return result;

	Q_strncpy( path, "c:file.ext", sizeof( path ));
	COM_StripExtension( path );
	return ExpectString( path, "c:file", 5 );
}

static int Test_DefaultAndReplaceExtension( void )
{
	char path[64];
	int result;

	Q_strncpy( path, "dir/file", sizeof( path ));
	COM_DefaultExtension( path, ".cfg", sizeof( path ));
	result = ExpectString( path, "dir/file.cfg", 1 );
	if( result ) return result;

	Q_strncpy( path, "dir/file.ext", sizeof( path ));
	COM_DefaultExtension( path, ".cfg", sizeof( path ));
	result = ExpectString( path, "dir/file.ext", 2 );
	if( result ) return result;

	Q_strncpy( path, "dir.ext/file", sizeof( path ));
	COM_DefaultExtension( path, ".cfg", sizeof( path ));
	result = ExpectString( path, "dir.ext/file.cfg", 3 );
	if( result ) return result;

	Q_strncpy( path, "dir/file.ext", sizeof( path ));
	COM_ReplaceExtension( path, ".cfg", sizeof( path ));
	result = ExpectString( path, "dir/file.cfg", 4 );
	if( result ) return result;

	Q_strncpy( path, ".nomedia", sizeof( path ));
	COM_ReplaceExtension( path, ".cfg", sizeof( path ));
	return ExpectString( path, ".nomedia.cfg", 5 );
}

static int Test_PathSlashFix( void )
{
	char path[64];
	int result;

	Q_strncpy( path, "dir", sizeof( path ));
	COM_PathSlashFix( path );
	result = ExpectString( path, "dir/", 1 );
	if( result ) return result;

	Q_strncpy( path, "dir\\", sizeof( path ));
	COM_PathSlashFix( path );
	result = ExpectString( path, "dir/", 2 );
	if( result ) return result;

	Q_strncpy( path, "dir/", sizeof( path ));
	COM_PathSlashFix( path );
	return ExpectString( path, "dir/", 3 );
}

static int Test_FixSlashes( void )
{
	char path[64] = "path\\\\with//mixed\\\\slashes";

	COM_FixSlashes( path );
	return ExpectString( path, "path/with/mixed/slashes", 1 );
}

int main( void )
{
	int result = Test_FileWithoutPath();

	if( result ) return result;

	result = Test_StripExtension();
	if( result ) return result + 16;

	result = Test_DefaultAndReplaceExtension();
	if( result ) return result + 32;

	result = Test_PathSlashFix();
	if( result ) return result + 48;

	result = Test_FixSlashes();
	if( result ) return result + 64;

	return EXIT_SUCCESS;
}
