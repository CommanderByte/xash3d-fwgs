#include <stdlib.h>

#include "filesystem/file_handle_ops.hpp"

using namespace xash::filesystem;

static bool TestLogicalPosition()
{
	FileHandleCursor cursor = {};

	cursor.position = 12;
	cursor.bufferLength = 8;
	cursor.bufferIndex = 3;
	cursor.realLength = 20;

	return FileHandleOps::logicalPosition(cursor) == 7 &&
		!FileHandleOps::isEof(cursor);
}

static bool TestEof()
{
	FileHandleCursor cursor = {};

	cursor.position = 16;
	cursor.bufferLength = 4;
	cursor.bufferIndex = 4;
	cursor.realLength = 16;

	return FileHandleOps::logicalPosition(cursor) == 16 &&
		FileHandleOps::isEof(cursor);
}

static bool TestBufferedSeekRange()
{
	FileHandleCursor cursor = {};

	cursor.position = 32;
	cursor.bufferLength = 16;
	cursor.bufferIndex = 8;
	cursor.realLength = 64;

	return FileHandleOps::canSeekWithinBuffer(cursor, 16) &&
		FileHandleOps::canSeekWithinBuffer(cursor, 32) &&
		!FileHandleOps::canSeekWithinBuffer(cursor, 15) &&
		!FileHandleOps::canSeekWithinBuffer(cursor, 33) &&
		FileHandleOps::bufferIndexForTarget(cursor, 20) == 4;
}

static bool TestSeekTargetResolution()
{
	FileHandleCursor cursor = {};
	fs_offset_t target = -1;

	cursor.position = 32;
	cursor.bufferLength = 16;
	cursor.bufferIndex = 8;
	cursor.realLength = 64;

	if (FileHandleOps::resolveSeekTarget(cursor, 9, FileSeekOrigin::Set, target) !=
			FileSeekStatus::Ok ||
		target != 9)
	{
		return false;
	}

	if (FileHandleOps::resolveSeekTarget(cursor, -4, FileSeekOrigin::Current, target) !=
			FileSeekStatus::Ok ||
		target != 20)
	{
		return false;
	}

	if (FileHandleOps::resolveSeekTarget(cursor, -5, FileSeekOrigin::End, target) !=
			FileSeekStatus::Ok ||
		target != 59)
	{
		return false;
	}

	return FileHandleOps::resolveSeekTarget(cursor, -1, FileSeekOrigin::Set, target) ==
			FileSeekStatus::OutOfRange &&
		FileHandleOps::resolveSeekTarget(cursor, 1, FileSeekOrigin::End, target) ==
			FileSeekStatus::OutOfRange;
}

int main()
{
	if (!TestLogicalPosition() ||
		!TestEof() ||
		!TestBufferedSeekRange() ||
		!TestSeekTargetResolution())
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
