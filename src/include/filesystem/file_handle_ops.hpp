#ifndef XASH_FILESYSTEM_FILE_HANDLE_OPS_HPP
#define XASH_FILESYSTEM_FILE_HANDLE_OPS_HPP

#include "xash3d_types.h"

namespace xash
{
namespace filesystem
{

enum class FileSeekOrigin
{
	Set,
	Current,
	End,
};

enum class FileSeekStatus
{
	Ok,
	InvalidOrigin,
	OutOfRange,
};

struct FileHandleCursor
{
	fs_offset_t position;
	fs_offset_t bufferLength;
	fs_offset_t bufferIndex;
	fs_offset_t realLength;
};

class FileHandleOps
{
public:
	static fs_offset_t logicalPosition(const FileHandleCursor &cursor);
	static fs_offset_t length(const FileHandleCursor &cursor);
	static bool isEof(const FileHandleCursor &cursor);
	static bool canSeekWithinBuffer(
		const FileHandleCursor &cursor,
		fs_offset_t target);
	static fs_offset_t bufferIndexForTarget(
		const FileHandleCursor &cursor,
		fs_offset_t target);
	static FileSeekStatus resolveSeekTarget(
		const FileHandleCursor &cursor,
		fs_offset_t offset,
		FileSeekOrigin origin,
		fs_offset_t &target);
};

}
}

#endif
