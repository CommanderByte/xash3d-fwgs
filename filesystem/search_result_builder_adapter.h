#ifndef XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_ADAPTER_H
#define XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_ADAPTER_H

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

void FS_SearchResult_Sort( stringlist_t *list );
size_t FS_SearchResult_PackedStringBytes( const stringlist_t *list );
void FS_SearchResult_CopyPacked( const stringlist_t *list, search_t *search );

#ifdef __cplusplus
}
#endif

#endif
