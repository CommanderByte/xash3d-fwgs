#ifndef XASH_FILESYSTEM_FILE_HANDLE_OPS_ADAPTER_H
#define XASH_FILESYSTEM_FILE_HANDLE_OPS_ADAPTER_H

#include "filesystem/compat/private/filesystem_private_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef fs_offset_t ( *fs_file_read_fn )( file_t *file, void *buffer,
	size_t buffersize );

fs_offset_t FS_FileHandleTell( const file_t *file );
fs_offset_t FS_FileHandleLength( const file_t *file );
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
int FS_FileHandleGetc( file_t *file, fs_file_read_fn read );
int FS_FileHandleUnGetc( file_t *file, int c );
int FS_FileHandleGets( file_t *file, char *string, size_t bufsize,
	fs_file_read_fn read );

#ifdef __cplusplus
}
#endif

#endif
