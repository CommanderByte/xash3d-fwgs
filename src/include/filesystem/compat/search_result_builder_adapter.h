#ifndef XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_ADAPTER_H
#define XASH_FILESYSTEM_SEARCH_RESULT_BUILDER_ADAPTER_H

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

void FS_SearchResult_Prepare( stringlist_t *list );
size_t FS_SearchResult_PackedStringBytes( const stringlist_t *list );
size_t FS_SearchResult_AllocationBytes( const stringlist_t *list );
void FS_SearchResult_Init( const stringlist_t *list, search_t *search );

#ifdef __cplusplus
}
#endif

#endif
