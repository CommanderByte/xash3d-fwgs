#include "filesystem/compat/file_handle_ops_adapter.h"

#include <stdio.h>

#include "filesystem/file_handle_ops.hpp"

using xash::filesystem::FileHandleCursor;
using xash::filesystem::FileHandleOps;
using xash::filesystem::FileSeekOrigin;
using xash::filesystem::FileSeekStatus;

namespace
{

FileHandleCursor CursorFromFile( const file_t *file )
{
	FileHandleCursor cursor = {};

	if( !file )
		return cursor;

	cursor.position = file->position;
	cursor.bufferLength = file->buff_len;
	cursor.bufferIndex = file->buff_ind;
	cursor.realLength = file->real_length;
	return cursor;
}

bool SeekOriginFromWhence( int whence, FileSeekOrigin &origin )
{
	switch( whence )
	{
	case SEEK_SET:
		origin = FileSeekOrigin::Set;
		return true;
	case SEEK_CUR:
		origin = FileSeekOrigin::Current;
		return true;
	case SEEK_END:
		origin = FileSeekOrigin::End;
		return true;
	default:
		return false;
	}
}

}

fs_offset_t FS_FileHandleTell( const file_t *file )
{
	if( !file )
		return 0;

	return FileHandleOps::logicalPosition( CursorFromFile( file ));
}

fs_offset_t FS_FileHandleLength( const file_t *file )
{
	if( !file )
		return 0;

	return FileHandleOps::length( CursorFromFile( file ));
}

qboolean FS_FileHandleEof( const file_t *file )
{
	if( !file )
		return true;

	return FileHandleOps::isEof( CursorFromFile( file )) ? true : false;
}

qboolean FS_FileHandleResolveSeek(
	const file_t *file,
	fs_offset_t offset,
	int whence,
	fs_offset_t *target )
{
	FileSeekOrigin origin;

	if( !file || !target || !SeekOriginFromWhence( whence, origin ))
		return false;

	return FileHandleOps::resolveSeekTarget(
		CursorFromFile( file ),
		offset,
		origin,
		*target ) == FileSeekStatus::Ok ? true : false;
}

qboolean FS_FileHandleCanSeekWithinBuffer(
	const file_t *file,
	fs_offset_t target )
{
	if( !file )
		return false;

	return FileHandleOps::canSeekWithinBuffer(
		CursorFromFile( file ),
		target ) ? true : false;
}

fs_offset_t FS_FileHandleBufferIndexForTarget(
	const file_t *file,
	fs_offset_t target )
{
	if( !file )
		return 0;

	return FileHandleOps::bufferIndexForTarget(
		CursorFromFile( file ),
		target );
}

int FS_FileHandleGetc( file_t *file, fs_file_read_fn read )
{
	char c;

	if( !read || read( file, &c, 1 ) != 1 )
		return EOF;

	return c;
}

int FS_FileHandleUnGetc( file_t *file, int c )
{
	if( !file )
		return EOF;

	if( file->ungetc != EOF )
		return EOF;

	file->ungetc = c;
	return c;
}

int FS_FileHandleGets( file_t *file, char *string, size_t bufsize,
	fs_file_read_fn read )
{
	int c;
	size_t end = 0;

	if( !string || bufsize == 0 )
		return EOF;

	while( 1 )
	{
		c = FS_FileHandleGetc( file, read );

		if( c == '\r' || c == '\n' || c < 0 )
			break;

		if( end < bufsize - 1 )
			string[end++] = (char)c;
	}
	string[end] = 0;

	if( c == '\r' )
	{
		c = FS_FileHandleGetc( file, read );

		if( c != '\n' )
			FS_FileHandleUnGetc( file, c );
	}

	return c;
}
