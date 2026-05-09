#ifndef XASH_FILESYSTEM_DEBUG_SNAPSHOT_HPP
#define XASH_FILESYSTEM_DEBUG_SNAPSHOT_HPP

#include <stddef.h>
#include <stdint.h>

#include "debugging/debug_sink.hpp"
#include "debugging/debug_types.hpp"

namespace xash
{
namespace filesystem
{
namespace debugging
{

enum class FilesystemMountType
{
	Directory,
	Pak,
	Wad,
	Zip,
	Pk3Directory,
	AndroidAssets,
	Unknown,
};

struct FilesystemMountRecord
{
	unsigned order;
	FilesystemMountType type;
	const char *source;
	uint32_t flags;
	const char *flagsText;
	bool writable;
	bool archive;
	const char *mountReason;
	const char *parentArchive;
};

struct FilesystemMountSnapshot
{
	xash::debugging::SnapshotHeader header;
	const FilesystemMountRecord *records;
	size_t recordCount;
};

const char *FilesystemMountTypeName(FilesystemMountType type);

xash::debugging::DebugStatus WriteHumanMountSnapshot(
	xash::debugging::IDebugSink &sink,
	const FilesystemMountSnapshot &snapshot);

xash::debugging::DebugStatus WriteJsonMountSnapshot(
	xash::debugging::IDebugSink &sink,
	const FilesystemMountSnapshot &snapshot);

}
}
}

#endif
