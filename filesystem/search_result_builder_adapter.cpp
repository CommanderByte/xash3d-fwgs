#include "search_result_builder_adapter.h"

#include "filesystem/search_result_builder.hpp"

using xash::filesystem::SearchResultBuilder;

void FS_SearchResult_Sort( stringlist_t *list )
{
	if( !list || !list->strings || list->numstrings <= 1 )
		return;

	SearchResultBuilder::sort( list->strings, (size_t)list->numstrings );
}

size_t FS_SearchResult_PackedStringBytes( const stringlist_t *list )
{
	if( !list || !list->strings || list->numstrings <= 0 )
		return 0;

	return SearchResultBuilder::packedStringBytes(
		list->strings,
		(size_t)list->numstrings );
}

void FS_SearchResult_CopyPacked( const stringlist_t *list, search_t *search )
{
	if( !list || !search || !list->strings || list->numstrings <= 0 )
		return;

	SearchResultBuilder::copyPacked(
		list->strings,
		(size_t)list->numstrings,
		search->filenames,
		search->filenamesbuffer );
}
