#include "filesystem/compat/search_result_builder_adapter.h"

#include "filesystem/compat/private/filesystem_private_memory.h"

#include "filesystem/search_result_builder.hpp"

#include <string.h>

using xash::filesystem::SearchResultBuilder;

void FS_SearchResult_Prepare( stringlist_t *list )
{
	if( !list || !list->strings || list->numstrings <= 1 )
		return;

	SearchResultBuilder::sort( list->strings, (size_t)list->numstrings );

	int write = 1;

	for( int read = 1; read < list->numstrings; ++read )
	{
		if( !strcmp( list->strings[write - 1], list->strings[read] ))
		{
			Mem_Free( list->strings[read] );
			continue;
		}

		list->strings[write++] = list->strings[read];
	}

	list->numstrings = write;
}

size_t FS_SearchResult_PackedStringBytes( const stringlist_t *list )
{
	if( !list || !list->strings || list->numstrings <= 0 )
		return 0;

	return SearchResultBuilder::packedStringBytes(
		list->strings,
		(size_t)list->numstrings );
}

size_t FS_SearchResult_AllocationBytes( const stringlist_t *list )
{
	if( !list || list->numstrings <= 0 )
		return 0;

	return sizeof( search_t ) +
		(size_t)list->numstrings * sizeof( char * ) +
		FS_SearchResult_PackedStringBytes( list );
}

void FS_SearchResult_Init( const stringlist_t *list, search_t *search )
{
	if( !list || !search || !list->strings || list->numstrings <= 0 )
		return;

	search->filenames = (char **)((char *)search + sizeof( search_t ));
	search->filenamesbuffer = (char *)((char *)search + sizeof( search_t ) +
		(size_t)list->numstrings * sizeof( char * ));
	search->numfilenames = list->numstrings;

	SearchResultBuilder::copyPacked(
		list->strings,
		(size_t)list->numstrings,
		search->filenames,
		search->filenamesbuffer );
}
