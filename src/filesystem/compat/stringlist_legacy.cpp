#define _GNU_SOURCE 1

#include "build.h"

#if XASH_WIN32
#include <io.h>
#elif XASH_DOS4GW
#include <direct.h>
#else
#include <dirent.h>
#endif

#include <string.h>

#include "port.h"
#include "crtlib.h"
#include "filesystem/compat/private/filesystem_private_api.h"
#include "common/com_strings.h"

extern "C" {

void stringlistinit( stringlist_t *list )
{
	memset( list, 0, sizeof( *list ));
}

void stringlistfreecontents( stringlist_t *list )
{
	int i;

	for( i = 0; i < list->numstrings; i++ )
	{
		if( list->strings[i] )
			Mem_Free( list->strings[i] );
		list->strings[i] = NULL;
	}

	if( list->strings )
		Mem_Free( list->strings );

	list->numstrings = 0;
	list->maxstrings = 0;
	list->strings = NULL;
}

void stringlistappend( stringlist_t *list, const char *text )
{
	size_t textlen;

	if( !Q_strcmp( text, "." ) || !Q_strcmp( text, ".." ))
		return; // ignore the virtual directories

	if( list->numstrings >= list->maxstrings )
	{
		list->maxstrings += 4096;
		list->strings = (char **)Mem_Realloc(
			fs_mempool,
			list->strings,
			list->maxstrings * sizeof( *list->strings ));
	}

	textlen = Q_strlen( text ) + 1;
	list->strings[list->numstrings] = (char *)Mem_Calloc( fs_mempool, textlen );
	memcpy( list->strings[list->numstrings], text, textlen );
	list->numstrings++;
}

void stringlistsort( stringlist_t *list )
{
	char *temp;
	int i, j;

	// this is a selection sort (finds the best entry for each slot)
	for( i = 0; i < list->numstrings - 1; i++ )
	{
		for( j = i + 1; j < list->numstrings; j++ )
		{
			if( Q_strcmp( list->strings[i], list->strings[j] ) > 0 )
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

#if XASH_DOS4GW
// convert names to lowercase because dos doesn't care, but pattern matching code often does
static void listlowercase( stringlist_t *list )
{
	char *c;
	int i;

	for( i = 0; i < list->numstrings; i++ )
	{
		for( c = list->strings[i]; *c; c++ )
			*c = Q_tolower( *c );
	}
}
#endif

void listdirectory( stringlist_t *list, const char *path, qboolean dirs_only )
{
#if XASH_WIN32
	char pattern[4096];
	struct _finddata_t n_file;
	intptr_t hFile;

	Q_snprintf( pattern, sizeof( pattern ), "%s/*", path );

	hFile = _findfirst( pattern, &n_file );
	if( hFile == -1 )
		return;

	stringlistappend( list, n_file.name );
	while( _findnext( hFile, &n_file ) == 0 )
	{
		if( dirs_only && !FBitSet( n_file.attrib, _A_SUBDIR ))
			continue;

		stringlistappend( list, n_file.name );
	}
	_findclose( hFile );
#else
	DIR *dir;
	struct dirent *entry;

	dir = opendir( path );

	if( !dir )
		return;

	while(( entry = readdir( dir )))
	{
#if HAVE_DIRENT_D_TYPE
		if( dirs_only && entry->d_type != DT_DIR && entry->d_type != DT_LNK && entry->d_type != DT_UNKNOWN )
			continue;
#endif

		stringlistappend( list, entry->d_name );
	}

	closedir( dir );
#endif

#if XASH_DOS4GW
	listlowercase( list );
#endif
}

}
