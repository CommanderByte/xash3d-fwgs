#include "filesystem/file_handle_ops.hpp"

namespace xash
{
namespace filesystem
{

fs_offset_t FileHandleOps::logicalPosition(const FileHandleCursor &cursor)
{
	return cursor.position - cursor.bufferLength + cursor.bufferIndex;
}

fs_offset_t FileHandleOps::length(const FileHandleCursor &cursor)
{
	return cursor.realLength;
}

bool FileHandleOps::isEof(const FileHandleCursor &cursor)
{
	return logicalPosition(cursor) == cursor.realLength;
}

bool FileHandleOps::canSeekWithinBuffer(
	const FileHandleCursor &cursor,
	fs_offset_t target)
{
	return cursor.position - cursor.bufferLength <= target &&
		target <= cursor.position;
}

fs_offset_t FileHandleOps::bufferIndexForTarget(
	const FileHandleCursor &cursor,
	fs_offset_t target)
{
	return target + cursor.bufferLength - cursor.position;
}

FileSeekStatus FileHandleOps::resolveSeekTarget(
	const FileHandleCursor &cursor,
	fs_offset_t offset,
	FileSeekOrigin origin,
	fs_offset_t &target)
{
	switch (origin)
	{
	case FileSeekOrigin::Current:
		target = logicalPosition(cursor) + offset;
		break;
	case FileSeekOrigin::Set:
		target = offset;
		break;
	case FileSeekOrigin::End:
		target = cursor.realLength + offset;
		break;
	default:
		return FileSeekStatus::InvalidOrigin;
	}

	if (target < 0 || target > cursor.realLength)
		return FileSeekStatus::OutOfRange;

	return FileSeekStatus::Ok;
}

}
}
