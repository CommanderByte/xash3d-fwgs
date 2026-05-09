#ifndef XASH_FILESYSTEM_FILE_HANDLE_OPS_ADAPTER_H
#define XASH_FILESYSTEM_FILE_HANDLE_OPS_ADAPTER_H

#include "filesystem_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

fs_offset_t FS_FileHandleTell( const file_t *file );
qboolean FS_FileHandleEof( const file_t *file );
qboolean FS_FileHandleResolveSeek(
	const file_t *file,
	fs_offset_t offset,
	int whence,
	fs_offset_t *target );
qboolean FS_FileHandleCanSeekWithinBuffer(
	const file_t *file,
	fs_offset_t target );
fs_offset_t FS_FileHandleBufferIndexForTarget(
	const file_t *file,
	fs_offset_t target );

#ifdef __cplusplus
}
#endif

#endif
