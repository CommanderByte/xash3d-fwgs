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

static qboolean WriteGameInfoFixtureWithFallback( const char *dir, const char *title, const char *basedir, const char *falldir )
{
	char path[256];
	char gameinfo[512];

	snprintf( gameinfo, sizeof( gameinfo ),
		"title \"%s\"\n"
		"basedir \"%s\"\n"
		"fallback_dir \"%s\"\n"
		"gamedll \"dlls/hl.dll\"\n",
		title, basedir, falldir );

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
	static const char fallback_only[] = "fallback only";
	static const char downloaded_only[] = "downloaded only";
	static const char hd_only[] = "hd only";
	static const char addon_only[] = "addon only";
	static const char lv_only[] = "lv only";
	static const char l10n_only[] = "l10n only";
	static const char custom_shared[] = "custom shared";

	if( !FS_TestCreateDirectory( "valve" ) ||
		!FS_TestCreateDirectory( "fallback" ) ||
		!FS_TestCreateDirectory( "mod" ) ||
		!FS_TestCreateDirectory( "mod_downloads" ) ||
		!FS_TestCreateDirectory( "mod_hd" ) ||
		!FS_TestCreateDirectory( "mod_addon" ) ||
		!FS_TestCreateDirectory( "mod_lv" ) ||
		!FS_TestCreateDirectory( "mod_fr" ) ||
		!FS_TestCreateDirectory( "mod/custom" ))
	{
		printf( "failed to create game hierarchy directories\n" );
		return false;
	}

	if( !WriteGameInfoFixture( "valve", "Base Game", "valve" ) ||
		!WriteGameInfoFixture( "fallback", "Fallback Game", "valve" ) ||
		!WriteGameInfoFixtureWithFallback( "mod", "Mod Game", "valve", "fallback" ))
		return false;

	if( !FS_TestWriteFile( "valve/shared.txt", base_shared, sizeof( base_shared ) - 1 ) ||
		!FS_TestWriteFile( "mod/shared.txt", mod_shared, sizeof( mod_shared ) - 1 ) ||
		!FS_TestWriteFile( "valve/baseonly.txt", base_only, sizeof( base_only ) - 1 ) ||
		!FS_TestWriteFile( "fallback/fallonly.txt", fallback_only, sizeof( fallback_only ) - 1 ) ||
		!FS_TestWriteFile( "mod_downloads/downloaded.txt", downloaded_only, sizeof( downloaded_only ) - 1 ) ||
		!FS_TestWriteFile( "mod_hd/hdonly.txt", hd_only, sizeof( hd_only ) - 1 ) ||
		!FS_TestWriteFile( "mod_addon/addononly.txt", addon_only, sizeof( addon_only ) - 1 ) ||
		!FS_TestWriteFile( "mod_lv/lvonly.txt", lv_only, sizeof( lv_only ) - 1 ) ||
		!FS_TestWriteFile( "mod_fr/l10nonly.txt", l10n_only, sizeof( l10n_only ) - 1 ) ||
		!FS_TestWriteFile( "mod/custom/shared.txt", custom_shared, sizeof( custom_shared ) - 1 ))
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

	g_fs.LoadGameInfo( FS_MOUNT_HD | FS_MOUNT_LV | FS_MOUNT_ADDON | FS_MOUNT_L10N, "fr" );

	if( !CheckLoadedText( "shared.txt", "custom shared", false ))
		return false;

	if( !CheckLoadedText( "baseonly.txt", "base only", false ))
		return false;

	if( !CheckLoadedText( "fallonly.txt", "fallback only", false ))
		return false;

	if( !CheckLoadedText( "downloaded.txt", "downloaded only", true ))
		return false;

	if( !CheckLoadedText( "hdonly.txt", "hd only", true ))
		return false;

	if( !CheckLoadedText( "addononly.txt", "addon only", true ))
		return false;

	if( !CheckLoadedText( "lvonly.txt", "lv only", true ))
		return false;

	if( !CheckLoadedText( "l10nonly.txt", "l10n only", true ))
		return false;

	if( g_fs.LoadFile( "baseonly.txt", NULL, true ))
	{
		printf( "gamedironly loaded base directory file\n" );
		return false;
	}

	if( !CheckLoadedText( "shared.txt", "custom shared", true ))
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
	snprintf( path, sizeof( path ), "%s/fallback/fallonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/fallback/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/gameinfo.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod_downloads/downloaded.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod_hd/hdonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod_addon/addononly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod_lv/lvonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod_fr/l10nonly.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/custom/shared.txt", root );
	FS_TestRemoveFile( path );
	snprintf( path, sizeof( path ), "%s/mod/custom", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/valve", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/fallback", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod_downloads", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod_hd", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod_addon", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod_lv", root );
	FS_TestRemoveDirectory( path );
	snprintf( path, sizeof( path ), "%s/mod_fr", root );
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
